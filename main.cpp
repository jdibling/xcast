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


class App : public msg::MessageHandler
{
public:
	App(const opts::Options& opts) 
	:	opts_(opts),
		server_queue_(new msg::MsgQueue),
		state_(run_state)
	{
	}
	void run();

	void HandleThreadDie(msg::ThreadDie*);
	void HandleRequestProgress(msg::RequestProgress*);
	void HandleProgressReport(msg::Progress*);
	void HandleTogglePause(msg::TogglePause*);

private:
	enum State { run_state, stop_state }		state_;
	opts::Options								opts_;
	shared_ptr<msg::MsgQueue>					server_queue_;
	typedef Thread<InterfaceProcessor>		InterfaceThread;
	typedef Thread<GroupProcessor>			GroupThread;
	typedef Threads<GroupProcessor>			GroupThreads;
	unique_ptr<InterfaceThread>					ifc_thread_;
	GroupThreads								grp_threads_;
};

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

void App::HandleRequestProgress(msg::RequestProgress* prog)
{
	cout << "STATS" << endl;

	// tell every proc thread to send stats
	for( GroupThreads::ThreadVec::iterator thread = grp_threads_.threads_.begin(); thread != grp_threads_.threads_.end(); ++thread )
	{
		thread->ctx_->oob_queue_->push(unique_ptr<msg::BasicMessage>(new msg::RequestProgress));
	}
}

void App::HandleProgressReport(msg::Progress* prog)
{
	cout << prog->packet_time_ << endl;
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
	///*** Start Interface Processor Thread ***///
	ifc_thread_ = unique_ptr<InterfaceThread>(new InterfaceThread(unique_ptr<InterfaceProcessor>(new InterfaceProcessor(server_queue_))));

	///*** Start Processor Threads (1 per channel-group) ***///
	for( int i = 0; i < 1; ++i )
	{
		grp_threads_.threads_.push_back(GroupThread(unique_ptr<GroupProcessor>(new GroupProcessor(this->opts_, server_queue_, false))));
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

	ifc_thread_->ctx_->oob_queue_->push(unique_ptr<msg::BasicMessage>(new msg::ThreadDie));
	ifc_thread_->join();

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

