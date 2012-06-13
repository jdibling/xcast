#include "Processor.h"

using namespace std;


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

GroupProcessor::Source::Source(const std::string& cap)
{
	// Open the cap file for read
//	vector<char> file_name(cap.begin(), cap.end());
	std::string cap_f = cap;
	unsigned rc = cap_.Open(&cap_f[0], true, false, false);
	if( rc == MIS::ERR::NOT_FOUND )
		throwx(dibcore::ex::misapi_error("CaptureApi::Open", rc, cap));
	// Get some file stats
	cap_.SeekFromEnd(0);
	int ttl_recs = 0;
	__int64 ttl_bytes = 0;
	cap_.GetFilePosition(&ttl_recs, &ttl_bytes);
	cap_.SeekFromBeginning(0);
	ttl_bytes_ = static_cast<uint64_t>(ttl_bytes);
}

GroupProcessor::Channel::Channel(const std::string& name, const std::string& cap_file, const std::string& group, unsigned short port)
:	name_(name),
	conn_(group, port),
	src_(cap_file)
{
}

void GroupProcessor::Init()
{
	for( opts::ChannelDescs::const_iterator desc = opts_.channels_.begin(); desc != opts_.channels_.end(); ++desc )
	{
		ChannelPtr ch(new Channel(desc->name_,desc->file_,desc->group_, desc->port_));
		channels_[desc->name_] = std::move(ch);
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

bool GroupProcessor::ComparePacketTimes(const ChannelPtrs::value_type& lhs, const ChannelPtrs::value_type& rhs)
{
	long lhs_ms = 0, rhs_ms = 0;
	lhs.second->src_.cap_.GetCurrentPacketTimeAsMS(&lhs_ms);
	rhs.second->src_.cap_.GetCurrentPacketTimeAsMS(&rhs_ms);

	return lhs_ms < rhs_ms;
}

void GroupProcessor::ProcessPacket()
{
	static uint64_t seq = 0;
	++seq;

	///***  SELECT NEXT PACKET TO SEND ***///
	ChannelPtrs::iterator it = std::min_element(channels_.begin(), channels_.end(), &GroupProcessor::ComparePacketTimes);
	if( it == channels_.end() )
		throwx(dibcore::ex::generic_error("Internal Malfunction"));

	///***  SEND PACKET ***///
	Channel& chan = *it->second.get();

	static const size_t packet_buf_sz = 1024 * 1024;
	static vector<char> packet_buf(packet_buf_sz, 0);

	int bytes_read = 0;
	unsigned rc = chan.src_.cap_.ReadPacket(&packet_buf[0], static_cast<unsigned>(packet_buf.size()), &bytes_read);
	switch( rc )
	{
	case MIS::ERR::SUCCESS :
		break;

	case MIS::ERR::END_OF_FILE :
		channels_.erase(it);
		return;

	default :
		throwx(dibcore::ex::misapi_error("CaptureApi.ReadPacket", rc, chan.name_));
	}

	size_t sent_bytes = chan.conn_.sock_.send_to(boost::asio::buffer(packet_buf, bytes_read), chan.conn_.group_);
	if( !sent_bytes )
			throwx(dibcore::ex::generic_error("No Bytes Sent"));

	boost::this_thread::sleep(boost::posix_time::milliseconds(5));
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
