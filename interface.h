#ifndef XCAST_INTERFACE
#define XCAST_INTERFACE

#include <cstdlib>
#include <memory>

#include "msg.h"

#if defined( WIN32 ) || defined( _WIN32 ) || defined( __WIN32__ )

class InterfaceProcessor : private msg::MessageHandler
{
public:
	InterfaceProcessor(std::shared_ptr<msg::MsgQueue> server_queue);
	InterfaceProcessor(InterfaceProcessor&& rhs);

	virtual ~InterfaceProcessor() {};
	void operator()() ;

private:
	inline void DebugMessage(const std::string& msg) const
	{
		server_queue_->push(std::unique_ptr<msg::DebugMessage>(new msg::DebugMessage(msg)));
	}

	void Init();
	void Teardown();

	void ProcessOOBEvent();
	void ProcessInterfaceEvent();
		void ProcessInterfaceEvent_KeyPress(const INPUT_RECORD& ir);
	void ProcessHeartBeat();

	void OnCommand(char c);

	void HandleThreadDie(const msg::ThreadDie& die);
	void HandleInternalCommand(const msg::InternalCommand& cmd);

	std::string ID() const;

	enum State { run_state, die_state } state_;

	typedef std::shared_ptr<msg::win32::SignalingMsgQueue>	OOBQueue;
	
	std::shared_ptr<msg::MsgQueue>					server_queue_;
public:
	friend class PipeProcessor;
	OOBQueue										oob_queue_;
	HANDLE											stdin_h_;
	DWORD											old_con_mode_;
	DWORD											file_type_;

};

#endif

#endif