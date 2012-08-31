#ifndef XCAST_MSG
#define XCAST_MSG

#include <cstdlib>
#include <cstdint>
#include <queue>
#include <boost/thread.hpp>
#include <boost/chrono.hpp>

#if defined( WIN32 ) || defined( _WIN32 ) || defined( __WIN32__ )
#	include <WinSock2.h>
#	include <Windows.h>
#	include <WinBase.h>
#endif

#include "xcast.h"



namespace msg
{
	class MessageHandler;

	class BasicMessage
	{
	public:
		virtual void Handle(MessageHandler&) = 0;
		virtual ~BasicMessage() = 0;
	};

	class LogMessage : public BasicMessage
	{
	public:
		void Handle(MessageHandler&) ;
		LogMessage(const std::string& msg) : msg_(msg) {};
		virtual ~LogMessage() {};
	
		std::string msg_;
	};

	class DebugMessage : public LogMessage
	{
	public:
		DebugMessage(const std::string& msg) : LogMessage(msg) {};
		void Handle(MessageHandler&);
	};

	typedef LogMessage ErrorMessage;

	class InternalCommand : public BasicMessage
	{
	public:
		void Handle(MessageHandler&);
		InternalCommand(const std::string& msg) : msg_(msg) {};
		virtual ~InternalCommand() {};
	
		std::string msg_;
	};

	class HeartBeat : public BasicMessage
	{
	public:
		void Handle(MessageHandler&);
		~HeartBeat() {};
	};

	class ThreadDie : public BasicMessage
	{
	public:
		void Handle(MessageHandler&);
		~ThreadDie() {};
	};

	class ThreadDead : public BasicMessage
	{
	public:
		void Handle(MessageHandler&);
		ThreadDead(const std::string id) : id_(id) {}
		~ThreadDead() {};
		std::string id_;
	};

	class RequestProgress : public BasicMessage
	{
	public:
		enum Type { total_progress, indiv_progress } type_;
		RequestProgress(Type type) : type_(type) {};

		void Handle(MessageHandler&);
	};

	class GroupProgress : public BasicMessage
	{
	public:
		GroupProgress() : cur_src_byte_(0), max_src_byte_(0), bytes_sent_(0) {};
		void Handle(MessageHandler&);

		std::string						group_;
		uint64_t						cur_src_byte_;
		uint64_t						max_src_byte_;
		uint64_t						bytes_sent_;
		boost::chrono::milliseconds		ttl_elapsed_;
		std::string						next_packet_;
		size_t							num_groups_;
	};

	class ChannelProgress : public BasicMessage
	{
	public:
		ChannelProgress() : cur_src_byte_(0), max_src_byte_(0), bytes_sent_(0) {};
		void Handle(MessageHandler&);

		std::string						group_;
		std::string						channel_;
		uint64_t						cur_src_byte_;
		uint64_t						max_src_byte_;
		uint64_t						bytes_sent_;
		std::string						packet_time_;
	};

	class TogglePause : public BasicMessage
	{
	public:
		void Handle(MessageHandler&);
	};

	class SetPauseState : public BasicMessage
	{
	public:
		SetPauseState(bool paused) : paused_(paused) {};
		~SetPauseState() {};
		void Handle(MessageHandler&);
		bool paused_;
	};

	class AutoPaused : public BasicMessage
	{
	public:
		AutoPaused(const xcast::PacketTime& pt, const std::string& grp, const std::string& chan)
			:	pt_(pt), grp_(grp), chan_(chan) 
		{
		};
		xcast::PacketTime pt_;
		std::string chan_, grp_;
		void Handle(MessageHandler&);
		~AutoPaused() {};
	};

	class Restart : public BasicMessage
	{
	public:
		Restart() {};
		virtual ~Restart() {};
		void Handle(MessageHandler&);
	};

	class Restarted : public BasicMessage
	{
	public:
		Restarted(const std::string& chan) : chan_(chan) {};
		virtual ~Restarted() {};
		void Handle(MessageHandler&);
		std::string chan_;
	};

	class Paused : public BasicMessage
	{
	public:
		Paused(const std::string& grp)
			:	grp_(grp)
		{
		};
		std::string grp_;
		void Handle(MessageHandler&);
		~Paused() {};
	};

	class Resumed : public BasicMessage
	{
	public:
		Resumed(const std::string& grp)
			:	grp_(grp)
		{
		};
		std::string grp_;
		void Handle(MessageHandler&);
		~Resumed() {};
	};

	class MessageHandler
	{
	public:
		virtual void HandleHeartBeat(const HeartBeat&) {};
		virtual void HandleLogMessage(const LogMessage&) {};
		virtual void HandleDebugMessage(const DebugMessage&) {};
		virtual void HandleThreadDie(const ThreadDie&) {};
		virtual void HandleThreadDead(const ThreadDead&) {};
		virtual void HandleGroupProgressReport(const GroupProgress&) {};
		virtual void HandleChannelProgressReport(const ChannelProgress&) {};
		virtual void HandleRequestProgress(const RequestProgress&) {};
		virtual void HandleAutoPaused(const AutoPaused&) {};
		virtual void HandleTogglePause(const TogglePause&) {};
		virtual void HandleSetPauseState(const SetPauseState&) {};
		virtual void HandleRestart(const Restart&) {};
		virtual void HandleRestarted(const Restarted&) {};
		virtual void HandlePaused(const Paused&) {};
		virtual void HandleResumed(const Resumed&) {};
		virtual void HandleInternalCommand(const InternalCommand&) {};
	};

	class non_signaling_policy
	{
	protected:
		void signal() const {};
		void reset() const {};
	};

	template<typename Data, class SignalingPolicy=non_signaling_policy>
	class concurrent_queue : public SignalingPolicy
	{
	public:
		typedef SignalingPolicy SignalingPolicy;

		std::queue<Data> the_queue;
		mutable boost::mutex the_mutex;
		boost::condition_variable the_condition_variable;
	public:
		concurrent_queue() {};
		virtual ~concurrent_queue() {};
		//concurrent_queue(concurrent_queue<Data>&& rhs) : the_mutex(the_mutex.), the_queue(std::move(rhs.the_queue)) {};

		void push(Data const& data)
		{
			boost::mutex::scoped_lock lock(the_mutex);
			the_queue.push(data);
			lock.unlock();
			the_condition_variable.notify_one();
			SignalingPolicy::signal();
		}

		void push(Data&& data)
		{
			boost::mutex::scoped_lock lock(the_mutex);
			the_queue.push(std::move(data));
			lock.unlock();
			the_condition_variable.notify_one();
			SignalingPolicy::signal();
		}

		bool empty() const
		{
			boost::mutex::scoped_lock lock(the_mutex);
			return the_queue.empty();
		}

		bool try_pop(Data& popped_value)
		{
			boost::mutex::scoped_lock lock(the_mutex);
			if(the_queue.empty())
			{
				return false;
			}
		
			popped_value=std::move(the_queue.front());
			the_queue.pop();
			switch( the_queue.empty() )
			{
			case true :
				SignalingPolicy::reset();
				break;
			case false :
				SignalingPoicy::signal();
				break;
			}

			return true;
		}

		void wait_and_pop(Data& popped_value)
		{
			boost::mutex::scoped_lock lock(the_mutex);
			while(the_queue.empty())
			{
				the_condition_variable.wait(lock);
			}
		
			popped_value=std::move(the_queue.front());
			the_queue.pop();
			switch( the_queue.empty() )
			{
			case true :
				SignalingPolicy::reset();
				break;
			case false :
				SignalingPolicy::signal();
				break;
			}
		}
	};

	typedef concurrent_queue<std::unique_ptr<BasicMessage>> MsgQueue;
#if defined( WIN32 ) || defined( _WIN32 ) || defined( __WIN32__ )
	namespace win32
	{
		typedef void* HANDLE;
		class signaling_policy
		{
		public:
			signaling_policy() : event_(::CreateEvent(0, 1, 0, 0)) {};
			~signaling_policy() { ::CloseHandle(event_); }
			HANDLE get_native_handle() const { return event_; }

		protected:
			void signal() const
			{
				::SetEvent(event_);
			}
			void reset() const
			{
				::ResetEvent(event_);
			}
		private:
			HANDLE event_;
		};
	
		typedef concurrent_queue<std::unique_ptr<BasicMessage>,win32::signaling_policy> SignalingMsgQueue;
	}

	
#	endif

};

#endif