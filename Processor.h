#ifndef XCAST_PROCESSOR
#define XCAST_PROCESSOR

#include <WinSock2.h>

#include <cstdlib>
#include <memory>
#include <unordered_map>
#include <list>

#include "msg.h"
#include "options.h"

#include <boost/asio.hpp>
#include <boost/chrono.hpp>

//#include <misapi.h>
//#include <MisTypes.h>
//#include <Directory.h>
//#include <Database/SourceContent.h>
//#include <Database\Time.h>
#include <CaptureApi.h>


class GroupProcessor : private msg::MessageHandler
{
public:
	GroupProcessor(const opts::Options& o, std::shared_ptr<msg::MsgQueue> server_queue, bool playback_paused=true);
	GroupProcessor(GroupProcessor&& rhs)
	:	opts_(std::move(rhs.opts_)),
		server_queue_(std::move(rhs.server_queue_)),
		oob_queue_(std::move(rhs.oob_queue_)),
		state_(std::move(rhs.state_)),
		channels_(std::move(rhs.channels_))
	{

	}

	virtual ~GroupProcessor() {};
	void operator()() ;

	std::unique_ptr<msg::MsgQueue>	oob_queue_;

private:
	void Init();
	void Teardown();

	void ProcessOOBQueue();
	void ProcessPacket();

	void HandleTogglePause(msg::TogglePause* toggle_pause);
	void HandleThreadDie(msg::ThreadDie* die);
	virtual void HandleRequestProgress(msg::RequestProgress* req_prog);

	enum State { play_state, pause_state, die_state } state_;

	std::shared_ptr<msg::MsgQueue>	server_queue_;
	opts::Options					opts_;

	class Conn
	{
	public:
		Conn(const std::string& group, unsigned short port);

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
		struct PacketTime
		{
			bool operator<(const PacketTime& rhs) const;
			int	m_, d_, hh_, mm_, ss_, ms_;
		};

		Source(const std::string& cap_file);
		const uint64_t		ttl_bytes_;
		std::vector<char>	cur_packet_;
		PacketTime			cur_packet_time_;
		uint64_t			cur_byte_;

		unsigned ReadNext();
	private:
		CaptureApi	cap_;
	};

	class Channel
	{
	public:
		Channel(const std::string& name, const std::string& cap_file, const std::string& group, unsigned short port);

		Conn		conn_;
		Source		src_;
		std::string	name_;
	private:

		Channel();
		Channel(const Channel&);
		Channel(Channel&& rhs);
	};

	void GatherStats(const Channel& chan, size_t bytes_sent);

	typedef std::unique_ptr<Channel> ChannelPtr;
	typedef std::map<std::string, ChannelPtr> ChannelPtrs;
	ChannelPtrs channels_;

	static bool ComparePacketTimes(const ChannelPtrs::value_type&, const ChannelPtrs::value_type&);

	typedef boost::chrono::steady_clock timer_clock;
	typedef boost::chrono::time_point<timer_clock> tp;
	tp playback_start_;

	struct BasicStat
	{
		BasicStat(tp send_time, size_t bytes_sent) : send_time_(send_time), bytes_sent_(bytes_sent) {}
		tp				send_time_;
		size_t			bytes_sent_;
	};
	typedef std::list<BasicStat> BasicStats;
	typedef std::string ChannelID;
	ChannelID GetChannelID(const Channel& rhs)
	{
		return rhs.name_;
	}
	typedef std::unordered_map<ChannelID, BasicStats> ChannelStats;
	ChannelStats raw_stats_;
};

#endif 