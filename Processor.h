#ifndef XCAST_PROCESSOR
#define XCAST_PROCESSOR

#include <WinSock2.h>

#include <cstdlib>
#include <memory>

#include "msg.h"
#include "options.h"

#include <boost/asio.hpp>

class GroupProcessor : private msg::MessageHandler
{
public:
	GroupProcessor(const opts::Options& o, std::shared_ptr<msg::MsgQueue> server_queue, bool playback_paused=true);
	GroupProcessor(GroupProcessor&& rhs)
	:	opts_(std::move(rhs.opts_)),
		server_queue_(std::move(rhs.server_queue_)),
		oob_queue_(std::move(rhs.oob_queue_)),
		state_(std::move(rhs.state_))		
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

	void HandlePausePlayback(msg::PausePlayback* pause);
	void HandleResumePlayback(msg::ResumePlayback* resume);
	void HandleThreadDie(msg::ThreadDie* die);

	enum State { play_state, pause_state, die_state } state_;

	std::shared_ptr<msg::MsgQueue>	server_queue_;
	opts::Options					opts_;

	struct Connection
	{
		boost::asio::ip::udp::endpoint group_;
	};

	struct Channel
	{
		Connection	conn_;
	};

	typedef std::map<std::string, Channel> Channels;
	Channels	channels_;

};

#endif 