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

namespace ip = boost::asio::ip;
using ip::udp;

GroupProcessor::Conn::Conn(const std::string& group, unsigned short port)
:	group_(ip::address::from_string(group), port),
	io_svc_(),
	sock_(io_svc_, group_.protocol())
{
	using namespace ip::multicast;
	sock_.set_option(hops(0));
	sock_.set_option(outbound_interface(ip::address_v4::from_string("127.0.0.1")));
}
GroupProcessor::Channel::Channel(const std::string& group, unsigned short port)
:	conn_(group, port)
{
}

void GroupProcessor::Init()
{
	for( opts::ChannelDescs::const_iterator desc = opts_.channels_.begin(); desc != opts_.channels_.end(); ++desc )
	{
		ChannelPtr ch(new Channel(desc->group_, desc->port_));
		channels_[desc->name_] = std::move(ch);
	}
}

void GroupProcessor::Teardown()
{


}
/*
	boost::asio::ip::address addr = boost::asio::ip::address::from_string(group);
	ch.conn_.group_ = unique_ptr<boost::asio::ip::udp::endpoint>(new boost::asio::ip::udp::endpoint(addr, port_));
	ch.conn_.io_svc_ = unique_ptr<boost::asio::io_service>(new boost::asio::io_service);
	ch.conn_.sock_ = unique_ptr<boost::asio::ip::udp::socket>(new boost::asio::ip::udp::socket(*ch.conn_.io_svc_.get(), ch.conn_.group_->protocol()));
	ch.conn_.sock_->set_option(boost::asio::ip::multicast::hops(0));	
	ch.conn_.sock_->set_option(boost::asio::ip::multicast::outbound_interface(boost::asio::ip::address_v4::from_string("127.0.0.1")));

	string buf;
	buf = dibcore::util::Formatter() 
		<< desc->name_ << "\t" << ch.conn_.group_->address().to_string() << " : " << ch.conn_.group_->port() << "\tTESTTESTTEST\n";

	size_t bytes = ch.conn_.sock_->send_to(boost::asio::buffer(buf), *ch.conn_.group_.get());
*/

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
	static uint64_t seq = 0;
	++seq;

	for( ChannelPtrs::iterator ch = channels_.begin(); ch != channels_.end(); ++ch )
	{
		string buf = dibcore::util::Formatter() 
			<< ch->first << "\t"
			<< "Packet #   " << setw(9) << setfill('0') << right << seq;
		size_t sent = ch->second->conn_.sock_.send_to(boost::asio::buffer(buf), ch->second->conn_.group_);
		if( !sent )
			bool bk = true;
	}
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
