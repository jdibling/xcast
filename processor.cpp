#include "Processor.h"

#include <fstream>
#include <sstream>
#include <unordered_map>
#include <list>
#include <numeric>
#include <functional>
using namespace std;

#include <core/core.h>
using dibcore::util::Formatter;

#include <boost/chrono.hpp>




GroupProcessor::GroupProcessor(const std::string& group_name, const opts::Options& o, std::shared_ptr<msg::MsgQueue> server_queue, bool playback_paused) 
:	group_name_(group_name),
	opts_(o),
	server_queue_(server_queue), 
	oob_queue_(std::unique_ptr<msg::MsgQueue>(new msg::MsgQueue)), 
	state_(pause_state)
{
	if( !playback_paused )
		oob_queue_->push(unique_ptr<msg::BasicMessage>((msg::BasicMessage*)new msg::TogglePause()));
};

void GroupProcessor::ProcessOOBQueue()
{
	///*** Get any OOB Messages First ***///
	while( state_ != die_state && !oob_queue_->empty() )
	{
		std::unique_ptr<msg::BasicMessage> oob;
		oob_queue_->wait_and_pop(oob);
		oob->Handle(*this);
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
:	cur_byte_(0),
	ttl_bytes_([&cap]() -> const uint64_t
{
	ifstream fs(cap, ios::in|ios::binary);
	size_t a = fs.tellg();
	fs.seekg(0, ios::end);
	size_t b = fs.tellg();
	return static_cast<uint64_t>(b-a);
}	()),
	packet_buf_(64 * 1024, 0),
	packet_size_(0)
{
	// Open the cap file for read
	std::string cap_f = cap;
	unsigned rc = cap_.Open(&cap_f[0], true, false, false);
	if( rc == MIS::ERR::NOT_FOUND )
		throwx(dibcore::ex::misapi_error("CaptureApi::Open", rc, cap));
	// Set up the read buffer
	packet_buf_.resize(packet_buf_.capacity());
	// Read the first packet in to the buffer
	ReadNext();
}

unsigned GroupProcessor::Source::ReadNext()
{
	size_t last_size = packet_size_;
	packet_size_ = 0;

	int bytes_read = 0;
	unsigned rc = cap_.ReadPacket(&packet_buf_[0], static_cast<unsigned>(packet_buf_.size()), &bytes_read);

	switch( rc )
	{
	case MIS::ERR::SUCCESS :
		cur_byte_ += last_size;
		packet_size_ = bytes_read;
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

void GroupProcessor::LogMessage(const std::string& msg) const
{
	server_queue_->push(unique_ptr<msg::LogMessage>(new msg::LogMessage(msg)));
}

void GroupProcessor::Teardown()
{
	unique_ptr<msg::ThreadDead> dead(new msg::ThreadDead);
	dead->id_ = group_name_;
	server_queue_->push(unique_ptr<msg::BasicMessage>(std::move(dead)));	
	LogMessage(Formatter() << "Processor ' " << group_name_ << "' Teardown Complete");
}

void GroupProcessor::HandleThreadDie(const msg::ThreadDie&)
{
	state_ = die_state;
}

void GroupProcessor::HandleSetPauseState(const msg::SetPauseState& set)
{
	if( !IsRunState() )
		return;

	SetPauseState(set.paused_);
	server_queue_->push(unique_ptr<msg::BasicMessage>(set.paused_ ? static_cast<msg::BasicMessage*>(new msg::Paused(group_name_)) : static_cast<msg::BasicMessage*>(new msg::Resumed(group_name_))));
}


void GroupProcessor::HandleTogglePause(const msg::TogglePause&)
{
	if( !IsRunState() )
		return;

	TogglePause();
	server_queue_->push(unique_ptr<msg::BasicMessage>(state_ == pause_state ? static_cast<msg::BasicMessage*>(new msg::Paused(group_name_)) : static_cast<msg::BasicMessage*>(new msg::Resumed(group_name_))));
}

void GroupProcessor::TogglePause()
{
	switch( state_ )
	{
	case pause_state :
		state_ = play_state;
		stats_.playback_start_ = Clock::now();
		break;
	case play_state :
		state_ = pause_state;
		stats_.prev_elapsed_ += (Clock::now() - stats_.playback_start_);
		break;
	default :
		break;
	}
}

void GroupProcessor::SetPauseState(bool paused)
{
	if( state_ == die_state )
		return;

	bool toggle = paused ? state_ == play_state : state_ == pause_state;
	if( toggle )
		TogglePause();
}

void GroupProcessor::HandleRequestProgress(const msg::RequestProgress& req)
{
	unique_ptr<msg::GroupProgress> grp_prog(new msg::GroupProgress());
	grp_prog->group_ = group_name_;
	grp_prog->ttl_elapsed_ = boost::chrono::duration_cast<boost::chrono::milliseconds>(stats_.prev_elapsed_ );
	if( state_ == play_state )
	{
		grp_prog->ttl_elapsed_ += boost::chrono::duration_cast<boost::chrono::milliseconds>(Clock::now() - stats_.playback_start_);
	}
	
	std::set<xcast::PacketTime> packet_times;

	for_each( channels_.begin(), channels_.end(), [this, &grp_prog, &packet_times, &req](ChannelPtrs::value_type& that)
	{
		unique_ptr<msg::ChannelProgress> ch_prog(new msg::ChannelProgress());
		const Channel& ch = *that.second.get();
		const GroupProcessor::Channel::Stats& stats = ch.stats_;

		ch_prog->channel_ = ch.name_;
		ch_prog->cur_src_byte_ = ch.src_.cur_byte_;
		ch_prog->max_src_byte_ = ch.src_.ttl_bytes_;
		ch_prog->group_ = grp_prog->group_;
		ch_prog->bytes_sent_ = stats.bytes_sent_;
		ch_prog->packet_time_ = ch.src_.cur_packet_time_.format();

		packet_times.insert(ch.src_.cur_packet_time_);

		grp_prog->bytes_sent_ += stats.bytes_sent_;
		grp_prog->cur_src_byte_ += ch.src_.cur_byte_;
		grp_prog->max_src_byte_ += ch.src_.ttl_bytes_;

		if( req.type_ == msg::RequestProgress::indiv_progress )
			server_queue_->push(unique_ptr<msg::BasicMessage>(std::move(ch_prog)));
	});

	if( !packet_times.empty() )
		grp_prog->next_packet_ = packet_times.begin()->format();

	if( req.type_ == msg::RequestProgress::total_progress )
		server_queue_->push(unique_ptr<msg::BasicMessage>(std::move(grp_prog)));
}

bool GroupProcessor::ComparePacketTimes(const ChannelPtrs::value_type& lhs, const ChannelPtrs::value_type& rhs)
{
	xcast::PacketTime 
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

	const xcast::PacketTime& cur_pt = it->second->src_.cur_packet_time_;

	// find next time to pause
	opts::PacketTimes::const_iterator next_pause = std::min_element(opts_.pauses_.begin(), opts_.pauses_.end(), std::less<xcast::PacketTime>());
	if( next_pause != opts_.pauses_.end() )
	{
		if( (*next_pause) < cur_pt )
		{
			///*** AUTO-PAUSE HERE ***///
			SetPauseState(true);
			server_queue_->push(unique_ptr<msg::AutoPaused>(new msg::AutoPaused(cur_pt, GetChannelID(*it->second), it->first)));
			opts_.pauses_.erase(next_pause);
			return;
		}
	}

	///***  SEND PACKET ***///
	Channel& chan = *it->second.get();
	size_t bytes_sent = chan.conn_.sock_.send_to(boost::asio::buffer(chan.src_.packet_buf_, chan.src_.packet_size_), chan.conn_.group_);
	if( !bytes_sent )
			throwx(dibcore::ex::generic_error("No Bytes Sent"));

	///*** ACCUM STATS ***///
	string s = GetChannelID(chan);
	chan.stats_.bytes_sent_ += bytes_sent;

	///*** ADVANCE TO NEXT PACKET ***///
	unsigned rc = chan.src_.ReadNext();
	if( rc == MIS::ERR::END_OF_FILE )
	{
		channels_.erase(it);
	}
}

void GroupProcessor::operator()() 
{
	try
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
				//boost::this_thread::sleep(boost::posix_time::milliseconds(5));
			}

			if( channels_.empty() )
				state_ = die_state;		
		}

		Teardown();
	}
	catch( const std::exception& ex )
	{
		cerr << ex.what() << endl;
		return;
	}
}
