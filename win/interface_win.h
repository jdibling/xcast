#ifndef XCAST_INTERFACE_WIN
#define XCAST_INTERFACE_WIN

#include <cstdlib>
#include <memory>

#include "../interface.h"
#include "../msg.h"

#if defined( WIN32 ) || defined( _WIN32 ) || defined( __WIN32__ )

namespace win
{
	class InterfaceProcessor : public ::InterfaceProcessor, private msg::MessageHandler
	{
	public:
		InterfaceProcessor(std::shared_ptr<msg::MsgQueue> server_queue);
		virtual ~InterfaceProcessor() {};
		void operator()() ;

		void PushOOBMessage(std::unique_ptr<msg::BasicMessage>&& msg)
		{
			oob_queue_->push(std::move(msg));
		}

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
};

#endif

#endif