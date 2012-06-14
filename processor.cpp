#include "Processor.h"

#include <fstream>
#include <sstream>
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
:	ttl_bytes_([&cap]() -> const uint64_t
{
	ifstream fs(cap, ios::in|ios::binary);
	size_t a = fs.tellg();
	fs.seekg(0, ios::end);
	size_t b = fs.tellg();
	return static_cast<uint64_t>(b-a);}())
{
	// Open the cap file for read
	std::string cap_f = cap;
	unsigned rc = cap_.Open(&cap_f[0], true, false, false);
	if( rc == MIS::ERR::NOT_FOUND )
		throwx(dibcore::ex::misapi_error("CaptureApi::Open", rc, cap));
	// Set up the read buffer
	cur_packet_.reserve(64 * 1024);
	cur_packet_.resize(0);
	// Read the first packet in to the buffer
	ReadNext();
}

unsigned GroupProcessor::Source::ReadNext()
{
	size_t last_size = cur_packet_.size();
	cur_packet_.resize(cur_packet_.capacity());

	int bytes_read = 0;
	unsigned rc = cap_.ReadPacket(&cur_packet_[0], static_cast<unsigned>(cur_packet_.capacity()), &bytes_read);

	switch( rc )
	{
	case MIS::ERR::SUCCESS :
		cur_byte_ += last_size;
		cur_packet_.resize(bytes_read);
		rc = cap_.GetCurrentPacketTime(&cur_packet_time_.m_, &cur_packet_time_.d_, &cur_packet_time_.hh_, &cur_packet_time_.mm_, &cur_packet_time_.ss_, &cur_packet_time_.ms_);
		if( rc != MIS::ERR::SUCCESS )
			throwx(dibcore::ex::misapi_error("CaptureApi.GetCurrentPacketTime", rc));
		break;

	case MIS::ERR::END_OF_FILE :
		break;

	default :
		throwx(dibcore::ex::misapi_error("CaptureApi.ReadPacket", rc));
	}

	return rc;
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

void GroupProcessor::HandleTogglePause(msg::TogglePause*)
{
	switch( state_ )
	{
	case pause_state :
		state_ = play_state;
		break;
	case play_state :
		state_ = pause_state;
		break;
	default :
		break;
	}
}

void GroupProcessor::HandleRequestProgress(msg::RequestProgress*)
{
	unique_ptr<msg::Progress> prog(new msg::Progress());

	// Find the next packet to go out, get the packet time.
	ChannelPtrs::iterator it = std::min_element(channels_.begin(), channels_.end(), &GroupProcessor::ComparePacketTimes);
	if( it == channels_.end() )
		throwx(dibcore::ex::generic_error("Internal Malfunction"));

	stringstream ss;
	const Source::PacketTime& pt = it->second->src_.cur_packet_time_;
	ss.width(2);
	ss.fill('0');
	ss << pt.m_ << "/" << pt.d_ << " " << pt.hh_ << ":" << pt.mm_ << ":" << pt.ss_ << "." << setw(6) << pt.ms_;
	ss.width(0);
	ss.fill(' ');
	ss << "\n";

	prog->packet_time_ = ss.str();

	// Push the message out
	server_queue_->push(unique_ptr<msg::BasicMessage>(std::move(prog)));
}

bool GroupProcessor::Source::PacketTime::operator<(const PacketTime& rhs) const
{
	if( m_ != rhs.m_ )		
		return m_ < rhs.m_;
	if( d_ != rhs.d_ )		
		return d_ < rhs.d_;
	if( hh_ != rhs.hh_ )	
		return hh_ < rhs.hh_;
	if( mm_ != rhs.mm_ )		
		return mm_ < rhs.mm_;
	if( ss_ != rhs.ss_ )		
		return ss_ < rhs.ss_;					
	return ms_ < rhs.ms_;
}

bool GroupProcessor::ComparePacketTimes(const ChannelPtrs::value_type& lhs, const ChannelPtrs::value_type& rhs)
{
	Source::PacketTime 
		&l_pt = lhs.second->src_.cur_packet_time_, 
		&r_pt = rhs.second->src_.cur_packet_time_;
	return l_pt < r_pt;
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
	size_t sent_bytes = chan.conn_.sock_.send_to(boost::asio::buffer(chan.src_.cur_packet_), chan.conn_.group_);
	if( !sent_bytes )
			throwx(dibcore::ex::generic_error("No Bytes Sent"));

	//const Source::PacketTime& pt = it->second->src_.cur_packet_time_;
	//cout << setw(20) << left << chan.name_ << right << "\t" << setw(4) << sent_bytes << "\t";
	//cout.width(2);
	//cout.fill('0');
	//cout << pt.m_ << "/" << pt.d_ << " " << pt.hh_ << ":" << pt.mm_ << ":" << pt.ss_ << "." << setw(6) << pt.ms_;
	//cout .width(0);
	//cout.fill(' ');
	//cout << endl;

	///*** ADVANCE TO NEXT PACKET ***///
	unsigned rc = chan.src_.ReadNext();
	if( rc == MIS::ERR::END_OF_FILE )
	{
		channels_.erase(it);
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

		if( state_ == play_state )
		{
			ProcessPacket();
			boost::this_thread::sleep(boost::posix_time::milliseconds(5));
		}

		if( channels_.empty() )
			state_ = die_state;		
	}

	Teardown();
}
