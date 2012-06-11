#include "Processor.h"

using namespace std;

#include <misapi.h>
#include <MisTypes.h>
#include <Directory.h>
#include <Database/SourceContent.h>
#include <Database\Time.h>
#include <CaptureApi.h>

#include <core/core.h>

GroupProcessor::GroupProcessor(const opts::Options& o, std::shared_ptr<msg::MsgQueue> server_queue, bool playback_paused) 
:	opts_(o),
	server_queue_(server_queue), 
	oob_queue_(std::unique_ptr<msg::MsgQueue>(new msg::MsgQueue)), 
	state_(playback_paused?pause_state:play_state)
{
};


void GroupProcessor::ProcessOOBQueue()
{
	///*** Get any OOB Messages First ***///
	while( state_ != die_state && !oob_queue_->empty() )
	{
		std::unique_ptr<msg::BasicMessage> oob;
		oob_queue_->wait_and_pop(oob);
		oob->Handle(this);
	}
}

void GroupProcessor::Init()
{
	for( opts::ChannelDescs::const_iterator ch = opts_.channels_.begin(); ch != opts_.channels_.end(); ++ch )
	{
		boost::asio::ip::address addr = boost::asio::ip::address::from_string(ch->group_);
		boost::asio::ip::udp::endpoint endpoint(addr, ch->port_);
		boost::asio::io_service io_svc;
		boost::asio::ip::udp::socket socket(io_svc, endpoint.protocol());
		socket.set_option(boost::asio::ip::multicast::hops(0));	
		socket.set_option(boost::asio::ip::multicast::outbound_interface(boost::asio::ip::address_v4::from_string("127.0.0.1")));

		string buf;
		buf = dibcore::util::Formatter() 
			<< ch->name_ << "\t" << endpoint.address().to_string() << " : " << endpoint.port() << "\tTESTTESTTEST\n";

		size_t bytes = socket.send_to(boost::asio::buffer(buf), endpoint);

		Channel ch;
		
	}
}

void GroupProcessor::Teardown()
{
}

void GroupProcessor::HandleThreadDie(msg::ThreadDie*)
{
	state_ = die_state;
}

void GroupProcessor::HandlePausePlayback(msg::PausePlayback*)
{
	state_ = pause_state;
}

void GroupProcessor::HandleResumePlayback(msg::ResumePlayback*)
{
	state_ = play_state;
}

void GroupProcessor::ProcessPacket()
{
	static uint64_t n = 0;
	++n;
}

void GroupProcessor::operator()() 
{
	Init();

	while( state_ != die_state )
	{
		ProcessOOBQueue();
		if( state_ == die_state )
			continue;

		ProcessPacket();
	}
}
