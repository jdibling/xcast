#include <cstdlib>
#include <stdio.h>
#include <iostream>
#include <iomanip>
#include <memory>
#include <list>
#include <vector>
#include <queue>

using namespace std;

#include "options.h"
#include "Channels.h"
#include "msg.h"
#include "Processor.h"
#include "interface.h"

#include <boost/thread.hpp>
#include "threads.h"

#include <Windows.h>

#include <core/core.h>
using dibcore::util::Formatter;

extern unsigned 
	ver_major = 0,
	ver_minor = 2,
	ver_build = 3;

string as_bytes(__int64 bytes, bool as_bits = false, std::streamsize width = 3)
{
	char b = as_bits ? 'b' : 'B';
	if( as_bits )
		bytes*=8;

	if( bytes < 1024 )
		return dibcore::util::Formatter() << setw(width) << bytes << " " << b;
	bytes /= 1024;
	if( bytes < 1024 )
		return Formatter() << setw(width) << bytes << " K" << b;
	bytes /= 1024;
	if( bytes < (1024*2) )
		return Formatter() << setw(width) << bytes << " M" << b;
	float bytes_f = bytes / 1024.0f;
	return Formatter() << setw(width) << setprecision(2) << bytes_f << " G" << b;
}


class App : public msg::MessageHandler
{
public:
	App(const opts::Options& opts) 
	:	opts_(opts),
		server_queue_(new msg::MsgQueue),
		state_(run_state),
		ifc_thread_(unique_ptr<InterfaceProcessor>(new InterfaceProcessor(server_queue_)))
	{
	}
	void run();

	void HandleLogMessage(const msg::LogMessage& log);
	void HandleThreadDie(const msg::ThreadDie&);
	void HandleThreadDead(const msg::ThreadDead&);
	void HandleRequestProgress(const msg::RequestProgress&);
	void HandleGroupProgressReport(const msg::GroupProgress&);
	void HandleChannelProgressReport(const msg::ChannelProgress&);
	void HandleTogglePause(const msg::TogglePause&);
	void HandleHeartBeat(const msg::HeartBeat&);
	void HandleAutoPaused(const msg::AutoPaused&);
	void HandlePaused(const msg::Paused&);
	void HandleResumed(const msg::Resumed&);

private:
	enum State { run_state, stop_state }		state_;
	opts::Options								opts_;
	shared_ptr<msg::MsgQueue>					server_queue_;
	typedef Thread<InterfaceProcessor>		InterfaceThread;
	typedef Thread<GroupProcessor>			GroupThread;
	typedef Threads<GroupProcessor>			GroupThreads;
	InterfaceThread							ifc_thread_;
	GroupThreads								grp_threads_;
};

void App::HandleLogMessage(const msg::LogMessage& log)
{
	cout << "MESSAGE: '" << log.msg_ << "'" << endl;
}

void App::HandleAutoPaused(const msg::AutoPaused& ap) 
{
	cout << "AUTO-PAUSED Group '" << ap.grp_ << "'. Next Packet On Channel '" << ap.chan_ << "' @ '" << ap.pt_.format() << "'." << endl;
}

void App::HandlePaused(const msg::Paused& p) 
{
	cout << "PAUSED Group '" << p.grp_ << "'." << endl;
}

void App::HandleResumed(const msg::Resumed& r) 
{
	cout << "RESUMED Group '" << r.grp_ << "'." << endl;
}

void App::HandleHeartBeat(const msg::HeartBeat&) 
{
	for( GroupThreads::ThreadVec::const_iterator thread = grp_threads_.threads_.begin(); thread != grp_threads_.threads_.end(); ++thread )
	{
		thread->ctx_->oob_queue_->push(unique_ptr<msg::BasicMessage>(new msg::RequestProgress(msg::RequestProgress::total_progress)));
	}

}
void App::HandleThreadDie(const msg::ThreadDie&) 
{
	cout << "QUIT" << endl;

	// tell every proc thread to shut down
	for( GroupThreads::ThreadVec::const_iterator thread = grp_threads_.threads_.begin(); thread != grp_threads_.threads_.end(); ++thread )
	{
		thread->ctx_->oob_queue_->push(unique_ptr<msg::BasicMessage>(new msg::ThreadDie));
	}

	grp_threads_.join_all();

	state_ = stop_state;
}

void App::HandleThreadDead(const msg::ThreadDead& dead) 
{
	string id = dead.id_;

	auto thread = find_if(grp_threads_.threads_.begin(), grp_threads_.threads_.end(), [&dead](const Thread<GroupProcessor>& that) -> bool
	{
		return that.ThreadID() == dead.id_;
	});

	cout << "Group '" << dead.id_ << "' EOF." << endl;
	thread->join();
	grp_threads_.threads_.erase(thread);
	if( grp_threads_.threads_.empty() )
	{
		state_ = stop_state;
	}
}

void App::HandleRequestProgress(const msg::RequestProgress& prog) 
{
	// tell every proc thread to send stats
	for( GroupThreads::ThreadVec::const_iterator thread = grp_threads_.threads_.begin(); thread != grp_threads_.threads_.end(); ++thread )
	{
		thread->ctx_->oob_queue_->push(unique_ptr<msg::BasicMessage>(new msg::RequestProgress(prog.type_)));
	}
}

void App::HandleGroupProgressReport(const msg::GroupProgress& prog) 
{
	int64_t speed = prog.bytes_sent_ / (int64_t)(ceil((double)prog.ttl_elapsed_.count() / 1000.0));
	cout << "***\t" 
		<< prog.group_ << "\t" 
		<< as_bytes(prog.bytes_sent_,true) << " Sent (%" << setw(3) << (int)floor((float(prog.cur_src_byte_)/float(prog.max_src_byte_))*100.0f) << ")" << "\t" 
		<< as_bytes(speed*8,true) << "/sec\t" 
		<< "Next: " << prog.next_packet_ << "\t"
		<< endl;
}

void App::HandleChannelProgressReport(const msg::ChannelProgress& prog) 
{
	cout << prog.group_ << ":" << setw(21) << prog.channel_ << "\t " 
		<< as_bytes(prog.cur_src_byte_,true) << " (%" << setw(3) << (int)floor((float(prog.cur_src_byte_)/float(prog.max_src_byte_))*100.0f) << ")" 
		<< "\tNext: " << prog.packet_time_ 
		<< endl; 
}

void App::HandleTogglePause(const msg::TogglePause&) 
{
	// tell every proc thread to send stats
	for( GroupThreads::ThreadVec::const_iterator thread = grp_threads_.threads_.begin(); thread != grp_threads_.threads_.end(); ++thread )
	{
		thread->ctx_->oob_queue_->push(unique_ptr<msg::BasicMessage>(new msg::TogglePause));
	}
}


void App::run()
{
	cout << "xcast v. " << ver_major << "." << ver_minor << " (Build " << ver_build << ")" << endl;
	///*** Start Processor Threads (1 per channel-group) ***///
	for( int i = 0; i < 1; ++i )
	{
		grp_threads_.threads_.push_back(GroupThread(unique_ptr<GroupProcessor>(new GroupProcessor("Group", this->opts_, server_queue_, false))));
		//GroupProcessor proc(server_queue_);
		//proc_threads_.add_thread(new boost::thread(proc));
	}	

	///*** WAIT FOR WORK ***///
	while( state_ == run_state )
	{
		unique_ptr<msg::BasicMessage> in_msg;
		server_queue_->wait_and_pop(in_msg);
		in_msg->Handle(*this);
	}

	ifc_thread_.ctx_->oob_queue_->push(unique_ptr<msg::BasicMessage>(new msg::ThreadDie));
	ifc_thread_.join();

	cout << "DONE" << endl;
}



int main(int ac, char* av[])
{
	/** Parse Options **/
	try
	{
		opts::Options o = opts::parse_command_line(ac, av);
		App app(o);
		app.run();
	}
	catch( const dibcore::ex::silent_exception& )
	{
		return 42;
	}
	catch( const dibcore::ex::generic_error& ex )
	{
		cerr << ex.what() << endl;
		return 43;
	}
	catch(...)
	{
		cerr << "Unhandled Exception" << endl;
		return 44;
	}
	
	return 0; 
}

