#ifndef XCAST_PROCESSOR
#define XCAST_PROCESSOR

//#include <WinSock2.h>

#include <cstdlib>
#include <memory>
#include <unordered_map>
#include <list>

#include "xcast.h"
#include "msg.h"
#include "options.h"

#include <boost/asio.hpp>
#include <boost/chrono.hpp>

#include <CaptureApi.h>


class GroupProcessor : private msg::MessageHandler
{
public:
	typedef boost::chrono::steady_clock Clock;
	typedef boost::chrono::nanoseconds ClockDuration;
	typedef boost::chrono::time_point<Clock,ClockDuration> TimePoint;

	GroupProcessor(const std::string& group_name, const opts::Options& o, std::shared_ptr<msg::MsgQueue> server_queue, bool playback_paused=true);
	GroupProcessor(GroupProcessor&& rhs)
	:	group_name_(std::move(rhs.group_name_)),
		opts_(std::move(rhs.opts_)),
		server_queue_(std::move(rhs.server_queue_)),
		oob_queue_(std::move(rhs.oob_queue_)),
		state_(std::move(rhs.state_)),
		channels_(std::move(rhs.channels_)),
		stats_(std::move(rhs.stats_))
	{

	}

	virtual ~GroupProcessor() {};
	void operator()() ;

	std::string ID() const { return group_name_; }

	std::unique_ptr<msg::MsgQueue>	oob_queue_;
	std::string group_name_;

private:
	void LogMessage(const std::string& msg) const;

	GroupProcessor(const GroupProcessor&);				// undefined -- not copy constructible
	GroupProcessor();									// undefined -- not default constructible
	GroupProcessor& operator=(const GroupProcessor&);	// undefined -- not copy assignable

	void Init();
	void Teardown();

	void ProcessOOBQueue();
	void ProcessPacket();

	void HandleTogglePause(const msg::TogglePause& toggle_pause);
		void TogglePause();
		void SetPauseState(bool paused);
	void HandleThreadDie(const msg::ThreadDie& die);
	void HandleRequestProgress(const msg::RequestProgress& req_prog);
	void HandleSetPauseState(const msg::SetPauseState& set);
	void HandleRestart(const msg::Restart&);

	enum State { play_state, pause_state, die_state } state_;
	inline bool IsRunState() { return state_ != die_state; }

	std::shared_ptr<msg::MsgQueue>	server_queue_;
	opts::Options					opts_;

	class Conn
	{
	public:
		Conn(const std::string& group, unsigned short port, unsigned ttl);

		boost::asio::ip::udp::endpoint		group_;
		boost::asio::io_service				io_svc_;
		boost::asio::ip::udp::socket		sock_;
		
	private:
		Conn();
		Conn(const Conn& rhs);
		Conn(Conn&&);
	};

	class Source
	{
	public:

		Source() : ttl_bytes_(0), cur_byte_(0) {};
		Source(const std::string& cap_file);
		const uint64_t		ttl_bytes_;				// cap file total size
		uint64_t			cur_byte_;				// current file position
//		std::vector<char>	cur_packet_;
		std::vector<char>	packet_buf_;			// packet buffer
		uint64_t			packet_size_;			// size of current packet

		xcast::PacketTime	cur_packet_time_;		// time of current (next) packet

		unsigned ReadNext();
		void Restart();

	private:
		CaptureApi	cap_;
	};

	class Channel
	{
	public:
		Channel(const std::string& name, const std::string& cap_file, const std::string& group, unsigned short port, unsigned ttl);

		Conn		conn_;
		Source		src_;
		std::string	name_;

		// Channel-Level Stats
		struct Stats
		{
			Stats() : bytes_sent_(0) {}
			uint64_t					bytes_sent_;
		} stats_;
	private:

		Channel();
		Channel(const Channel&);
		Channel(Channel&& rhs);
	};

	// Group-Level Stats
	struct Stats
	{
		TimePoint					playback_start_;
		ClockDuration				prev_elapsed_;
	} stats_;

	void GatherStats(const Channel& chan, size_t bytes_sent);

	typedef std::unique_ptr<Channel> ChannelPtr;
	typedef std::map<std::string, ChannelPtr> ChannelPtrs;
	ChannelPtrs channels_;

	static bool ComparePacketTimes(const ChannelPtrs::value_type&, const ChannelPtrs::value_type&);

	typedef std::string ChannelID;
	ChannelID GetChannelID(const Channel& rhs)
	{
		return rhs.name_;
	}
};

#endif 