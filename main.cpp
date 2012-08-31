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

/***  VERSION HISTORY	***

	Minor/Build		Comments

	3.4				Finished piping support for control from Java

*****************************/

string as_bytes(__int64 bytes, bool as_bits = false, std::streamsize width = 4)
{
	char b = as_bits ? 'b' : 'B';
	if( as_bits )
		bytes*=8;

	if( bytes < 1024 )
		return dibcore::util::Formatter() << setw(width) << bytes << "  " << b;
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
		ifc_thread_(unique_ptr<InterfaceProcessor>(new InterfaceProcessor(server_queue_))),
		stdout_(GetStdHandle(STD_OUTPUT_HANDLE)),
		stderr_(GetStdHandle(STD_ERROR_HANDLE))
	{
	}
	void run();

	void HandleLogMessage(const msg::LogMessage& log);
	void HandleDebugMessage(const msg::DebugMessage& log);
	void HandleThreadDie(const msg::ThreadDie&);
	void HandleThreadDead(const msg::ThreadDead&);
	void HandleRequestProgress(const msg::RequestProgress&);
	void HandleGroupProgressReport(const msg::GroupProgress&);
	void HandleChannelProgressReport(const msg::ChannelProgress&);
	void HandleTogglePause(const msg::TogglePause&);
	void HandleHeartBeat(const msg::HeartBeat&);
	void HandleAutoPaused(const msg::AutoPaused&);
	void HandleRestart(const msg::Restart&);
	void HandleRestarted(const msg::Restarted&);
	void HandlePaused(const msg::Paused&);
	void HandleResumed(const msg::Resumed&);

private:
	void DebugMessage(const string& msg) const
	{
		if( opts_.out_fmt_ == opts::verbose_fmt  )
		{
			string copy = msg;
			copy += "\n";
			DWORD written = 0;
			WriteFile(stderr_, copy.c_str(), static_cast<DWORD>(copy.length()), &written, 0);
		}
	}
	void LogMessage(const string& msg) const
	{
		string copy = msg;
		copy += "\n";
		DWORD written = 0;
		WriteFile(stdout_, copy.c_str(), static_cast<DWORD>(copy.length()), &written, 0);
	}

	void TermInterface();

	enum State { run_state, stop_state }		state_;
	opts::Options								opts_;
	shared_ptr<msg::MsgQueue>					server_queue_;
	typedef Thread<InterfaceProcessor>		InterfaceThread;
	typedef Thread<GroupProcessor>			GroupThread;
	typedef Threads<GroupProcessor>			GroupThreads;
	InterfaceThread							ifc_thread_;
	GroupThreads								grp_threads_;
	HANDLE									stdout_,		
											stderr_;
};

void App::TermInterface()
{
	DebugMessage("TermInterface Enter");

	ifc_thread_.ctx_->oob_queue_->push(unique_ptr<msg::ThreadDie>(new msg::ThreadDie()));

	DebugMessage("TermInterface Leave");
}

void App::HandleLogMessage(const msg::LogMessage& log)
{
	LogMessage(log.msg_);
}

void App::HandleDebugMessage(const msg::DebugMessage& msg)
{
	if( opts_.out_fmt_ == opts::verbose_fmt  )
		LogMessage(msg.msg_);
}

void App::HandleAutoPaused(const msg::AutoPaused& ap) 
{
	LogMessage(Formatter()
		<< "AUTO-PAUSED Group '" << ap.grp_ << "'. "
		<< "Next Packet On Channel '" << ap.chan_ << "' @ '" << ap.pt_.format() << "'.");
}

void App::HandlePaused(const msg::Paused& p) 
{
	LogMessage(Formatter()<<"PAUSED Group '" << p.grp_ << "'.");
}

void App::HandleResumed(const msg::Resumed& r) 
{
	LogMessage(Formatter() << "RESUMED Group '" << r.grp_ << "'.");
}

void App::HandleHeartBeat(const msg::HeartBeat&) 
{
	DebugMessage("HB");
	for( GroupThreads::ThreadVec::const_iterator thread = grp_threads_.threads_.begin(); thread != grp_threads_.threads_.end(); ++thread )
	{
		if( opts_.out_fmt_ == opts::verbose_fmt  )
			thread->ctx_->oob_queue_->push(unique_ptr<msg::BasicMessage>(new msg::RequestProgress(msg::RequestProgress::indiv_progress)));
		thread->ctx_->oob_queue_->push(unique_ptr<msg::BasicMessage>(new msg::RequestProgress(msg::RequestProgress::total_progress)));
	}

}
void App::HandleThreadDie(const msg::ThreadDie&) 
{
	DebugMessage("QUIT");

	// tell every proc thread to shut down
	for( GroupThreads::ThreadVec::const_iterator thread = grp_threads_.threads_.begin(); thread != grp_threads_.threads_.end(); ++thread )
	{
		thread->ctx_->oob_queue_->push(unique_ptr<msg::BasicMessage>(new msg::ThreadDie));
	}

	grp_threads_.join_all();

	DebugMessage("All Processors Terminated");
}

void App::HandleThreadDead(const msg::ThreadDead& dead) 
{
	string id = dead.id_;

	auto thread = find_if(grp_threads_.threads_.begin(), grp_threads_.threads_.end(), [&dead](const Thread<GroupProcessor>& that) -> bool
	{
		return that.ThreadID() == dead.id_;
	});
	if( thread != grp_threads_.threads_.end() )
	{
		// A group processor died
		thread->join();
		DebugMessage(Formatter() << "Group '" << dead.id_ << "' EOF.");
		grp_threads_.threads_.erase(thread);
		if( grp_threads_.threads_.empty() )
		{
			DebugMessage("All Groups EOF");
			TermInterface();
		}
	}
	else
	{
		// The interface processor died
		DebugMessage("Interface Processor Exited. Shutting down");
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
	if( opts_.show_type_ == opts::show_channels )
		return;

	if( opts_.out_fmt_ == opts::raw_fmt )
	{
		LogMessage( Formatter() 
			<< prog.group_ << " (TOTAL, " << prog.num_groups_ << " Groups)" << ","
			<< "Sent=" << prog.bytes_sent_ << ","
			<< "Cur=" << prog.cur_src_byte_ << ","
			<< "Max=" << prog.max_src_byte_ << ","
			<< "Elapsed=" << prog.ttl_elapsed_ << ","
			<< "Next=" << prog.next_packet_
			);
	}
	else
	{
		int64_t speed = 0;
		if( prog.ttl_elapsed_.count() > 0 )
			speed = prog.bytes_sent_ / (int64_t)(ceil((double)prog.ttl_elapsed_.count() / 1000.0));
	
		string chid = Formatter() << prog.group_ << " (TOTAL)" << ",";
		LogMessage(Formatter() 
			<< setw(21) << left << chid << setw(0) << " " 
			<< as_bytes(prog.bytes_sent_,true) << " " << setw(3) << right << (int)floor((float(prog.cur_src_byte_)/float(prog.max_src_byte_))*100.0f) << "%" << "\t" 
			<< as_bytes(speed*8,true) << "/sec" 
			<< " Next: " << prog.next_packet_ );
	}
}

void App::HandleChannelProgressReport(const msg::ChannelProgress& prog) 
{
	if( opts_.out_fmt_ == opts::raw_fmt )
	{
		LogMessage( Formatter()
			<< prog.group_ << "/" << prog.channel_ << ","
			<< "Sent=" << prog.bytes_sent_  << ","
			<< "Cur=" << prog.cur_src_byte_ << ","
			<< "Max=" << prog.max_src_byte_ << ","
			<< "Next=" << prog.packet_time_ 
		);
	}
	else
	{
		string chid = Formatter() << prog.group_ << "/" << prog.channel_;
		LogMessage(Formatter() 
			<< setw(21) << left << chid << setw(0) << " " 
			<< as_bytes(prog.cur_src_byte_,true) << " " << setw(3) << right << (int)floor((float(prog.cur_src_byte_)/float(prog.max_src_byte_))*100.0f) << "%" 
			<< " Next: " << prog.packet_time_ );
	}
}

void App::HandleTogglePause(const msg::TogglePause&) 
{
	// tell every proc thread to send stats
	for( GroupThreads::ThreadVec::const_iterator thread = grp_threads_.threads_.begin(); thread != grp_threads_.threads_.end(); ++thread )
	{
		thread->ctx_->oob_queue_->push(unique_ptr<msg::BasicMessage>(new msg::TogglePause));
	}
}

void App::HandleRestart(const msg::Restart&)
{
	// tell every proc thread to restart
	for( GroupThreads::ThreadVec::const_iterator thread = grp_threads_.threads_.begin(); thread != grp_threads_.threads_.end(); ++thread )
	{
		thread->ctx_->oob_queue_->push(unique_ptr<msg::Restart>(new msg::Restart));
	}
}

void App::HandleRestarted(const msg::Restarted& rst)
{
	LogMessage(Formatter() << "RESTARTED Group '" << rst.chan_ << "'.");
}

void App::run()
{
	DebugMessage("App Start");

	///*** Start Processor Threads (1 per channel-group) ***///
	for( int i = 0; i < 1; ++i )
	{
		grp_threads_.threads_.push_back(GroupThread(unique_ptr<GroupProcessor>(new GroupProcessor("Group", this->opts_, server_queue_, false))));
	}	

	LogMessage("=== XCAST PARAMETERS ===");
	LogMessage(Formatter()<<opts_.channels_.size() << " Channels:");
	for_each(opts_.channels_.begin(), opts_.channels_.end(), [this](const opts::ChannelDesc& ch)
	{
		this->LogMessage(Formatter() << "\t" << ch.name_ << "\t[" << ch.group_ << ":" << ch.port_ << "] : " << ch.file_);
	});
	LogMessage(Formatter()<<opts_.pauses_.size() << " Pauses:");
	for_each(opts_.pauses_.begin(), opts_.pauses_.end(), [this](const xcast::PacketTime& pt)
	{
		this->LogMessage(Formatter() << "\t" << pt.format());
	});
	LogMessage(Formatter() << "TTL: " << opts_.ttl_);
	LogMessage(Formatter() << "Packet Delay: " << opts_.delay_);
	LogMessage("=========================");

	DebugMessage("App Loop Enter");
	///*** WAIT FOR WORK ***///
	while( state_ == run_state )
	{
		DebugMessage("App Loop");

		unique_ptr<msg::BasicMessage> in_msg;
		server_queue_->wait_and_pop(in_msg);
		in_msg->Handle(*this);
	}
	DebugMessage("App Loop Exit");

	ifc_thread_.join();

	LogMessage("DONE");
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
		printf(ex.what());
		return 43;
	}
	catch( const std::exception& ex )
	{
		cerr << "Unhandled Exception: '" << ex.what() << "'" << endl;
		return 45;
	}
	catch(...)
	{
		cerr << "Unhandled Exception" << endl;
		return 44;
	}
	
	return 0; 
}

