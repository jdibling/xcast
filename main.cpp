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
	if( bytes < 1024 )
		return Formatter() << setw(width) << bytes << " M" << b;
	bytes /= 1024;
	if( bytes < 1024 )
		return Formatter() << setw(width) << bytes << " G" << b;
	bytes /= 1024;
	return Formatter() << setw(width) << bytes << " T" << b;
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

	void HandleThreadDie(msg::ThreadDie*);
	void HandleThreadDead(msg::ThreadDead*);
	void HandleRequestProgress(msg::RequestProgress*);
	void HandleGroupProgressReport(msg::GroupProgress*);
	void HandleChannelProgressReport(msg::ChannelProgress*);
	void HandleTogglePause(msg::TogglePause*);
	void HandleHeartBeat(msg::HeartBeat*);

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

void App::HandleHeartBeat(msg::HeartBeat* hb)
{
	for( GroupThreads::ThreadVec::iterator thread = grp_threads_.threads_.begin(); thread != grp_threads_.threads_.end(); ++thread )
	{
		thread->ctx_->oob_queue_->push(unique_ptr<msg::BasicMessage>(new msg::RequestProgress(msg::RequestProgress::total_progress)));
	}

}
void App::HandleThreadDie(msg::ThreadDie* die)
{
	cout << "QUIT" << endl;

	// tell every proc thread to shut down
	for( GroupThreads::ThreadVec::iterator thread = grp_threads_.threads_.begin(); thread != grp_threads_.threads_.end(); ++thread )
	{
		thread->ctx_->oob_queue_->push(unique_ptr<msg::BasicMessage>(new msg::ThreadDie));
	}

	grp_threads_.join_all();

	state_ = stop_state;
}

void App::HandleThreadDead(msg::ThreadDead* dead)
{
	string id = dead->id_;

	auto thread = find_if(grp_threads_.threads_.begin(), grp_threads_.threads_.end(), [&dead](const Thread<GroupProcessor>& that) -> bool
	{
		return that.ThreadID() == dead->id_;
	});

	cout << "Group '" << dead->id_ << "' EOF." << endl;
	thread->join();
	grp_threads_.threads_.erase(thread);
	if( grp_threads_.threads_.empty() )
	{
		state_ = stop_state;
	}
}

void App::HandleRequestProgress(msg::RequestProgress* prog)
{
	// tell every proc thread to send stats
	for( GroupThreads::ThreadVec::iterator thread = grp_threads_.threads_.begin(); thread != grp_threads_.threads_.end(); ++thread )
	{
		thread->ctx_->oob_queue_->push(unique_ptr<msg::BasicMessage>(new msg::RequestProgress(prog->type_)));
	}
}

void App::HandleGroupProgressReport(msg::GroupProgress* prog)
{
	int64_t speed = prog->bytes_sent_ / (int64_t)(ceil((double)prog->ttl_elapsed_.count() / 1000.0));
	cout << "***\t" 
		<< prog->group_ << "\t" 
		<< as_bytes(prog->bytes_sent_,true) << " Sent (%" << setw(3) << (int)floor((float(prog->cur_src_byte_)/float(prog->max_src_byte_))*100.0f) << ")" << "\t" 
		<< as_bytes(speed) << "/sec\t" 
		<< "Next: " << prog->next_packet_ << "\t"
		<< endl;
}

void App::HandleChannelProgressReport(msg::ChannelProgress* prog)
{
	cout << prog->group_ << ":" << setw(21) << prog->channel_ << "\t " 
		<< as_bytes(prog->cur_src_byte_,true) << " (%" << setw(3) << (int)floor((float(prog->cur_src_byte_)/float(prog->max_src_byte_))*100.0f) << ")" 
		<< "\tNext: " << prog->packet_time_ 
		<< endl; 
}

void App::HandleTogglePause(msg::TogglePause* toggle)
{
	cout << "PAUSE/PLAY" << endl;

	// tell every proc thread to send stats
	for( GroupThreads::ThreadVec::iterator thread = grp_threads_.threads_.begin(); thread != grp_threads_.threads_.end(); ++thread )
	{
		thread->ctx_->oob_queue_->push(unique_ptr<msg::BasicMessage>(new msg::TogglePause));
	}
}


void App::run()
{
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
		in_msg->Handle(this);
	}

	ifc_thread_.ctx_->oob_queue_->push(unique_ptr<msg::BasicMessage>(new msg::ThreadDie));
	ifc_thread_.join();

	cout << "DONE" << endl;
}



int main(int ac, char* av[])
{
	/** Parse Options **/
	opts::Options o = opts::parse_command_line(ac, av);
	App app(o);
	app.run();
	
	return 0; 
}

