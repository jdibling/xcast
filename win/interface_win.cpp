#include "interface_win.h"
#include "../msg.h"
#include "../threads.h"
#include <windows.h>
using std::shared_ptr;
#include <core/core.h>
using namespace dibcore;
using dibcore::util::Formatter;

#include <iostream>
#include <memory>
using namespace std;

namespace win
{
	InterfaceProcessor::InterfaceProcessor(std::shared_ptr<msg::MsgQueue> server_queue) 
	:	server_queue_(server_queue), 
		oob_queue_(shared_ptr<msg::win32::SignalingMsgQueue>(new msg::win32::SignalingMsgQueue)),
		state_(run_state)
	{

	};

	void InterfaceProcessor::ProcessOOBEvent()
	{
		DebugMessage("ProcessOOB");
		///*** Get any OOB Messages First ***///
		while( state_ != die_state && !oob_queue_->empty() )
		{
			std::unique_ptr<msg::BasicMessage> oob;
			oob_queue_->wait_and_pop(oob);
			oob->Handle(*this);
		}
	}

	void InterfaceProcessor::ProcessInterfaceEvent()
	{
		INPUT_RECORD ir_buf[128];
		DWORD num_read = 0;

		if( !ReadConsoleInput(stdin_h_, ir_buf, sizeof(ir_buf)/sizeof(ir_buf[0]), &num_read) )
			throwx(dibcore::ex::windows_error(GetLastError(), "ReadConsoleInput"));

		for( DWORD i = 0; i < num_read; ++i )
		{
			INPUT_RECORD& ir = ir_buf[i];
			switch( ir.EventType )
			{
			case KEY_EVENT: 
				ProcessInterfaceEvent_KeyPress(ir);
				break;

			case MOUSE_EVENT :
				break;

			case WINDOW_BUFFER_SIZE_EVENT: 
				break; 

			case FOCUS_EVENT:  // disregard focus events 
				break;

			case MENU_EVENT:   // disregard menu events 
				break; 

			default: 
				throwx(ex::generic_error("Unhandled Input Event"));
				break; 
			}
		}
	}

	void InterfaceProcessor::ProcessInterfaceEvent_KeyPress(const INPUT_RECORD& ir)
	{
		if( !ir.Event.KeyEvent.bKeyDown )
		{
			if( ir.Event.KeyEvent.wVirtualKeyCode >= 'A' && ir.Event.KeyEvent.wVirtualKeyCode<= 'Z' )
				OnCommand(static_cast<char>(LOBYTE(ir.Event.KeyEvent.wVirtualKeyCode)));
		}
	}

	void InterfaceProcessor::OnCommand(char cmd)
	{
		bool handled = false;
		DebugMessage(Formatter() << "InterfaceProcessor Command '" << cmd << "'");
		switch( cmd )
		{
		case 'Q' :	// quit command
			server_queue_->push(unique_ptr<msg::ThreadDie>(new msg::ThreadDie));
			handled = true;
			break;

		case 'S' :	// stats command
			server_queue_->push(unique_ptr<msg::RequestProgress>(new msg::RequestProgress(opts::show_both)));
			handled = true;
			break;

		case 'P' :	// Pause
			server_queue_->push(unique_ptr<msg::TogglePause>(new msg::TogglePause));
			handled = true;
			break;

		case 'R' :	// Restart
			server_queue_->push(unique_ptr<msg::Restart>(new msg::Restart()));
			handled = true;
			break;

		default:
			break;
		}

		DebugMessage(Formatter() << "InterfaceProcessor Command '" << cmd << "' " << (handled?"Handled" : "Not Handled"));
	}

	void InterfaceProcessor::HandleInternalCommand(const msg::InternalCommand& cmd)
	{
		DebugMessage(Formatter() << "InterfaceProcessor HandleInternalCommand('" << cmd.msg_ << "')");

		for_each(cmd.msg_.begin(), cmd.msg_.end(), [this](char c)
		{
			OnCommand(c);
		});
	}

	void InterfaceProcessor::ProcessHeartBeat()
	{
		DebugMessage("InterfaceProcessor ProcessHeartBeat");
		server_queue_->push(unique_ptr<msg::HeartBeat>(new msg::HeartBeat()));
	}

	void InterfaceProcessor::HandleThreadDie(const msg::ThreadDie&)
	{
		DebugMessage("InterfaceProcessor HandleThreadDie");
		state_ = die_state;
	}

	void InterfaceProcessor::Init()
	{
		using namespace dibcore;

		DebugMessage("InterfaceProcessor Init");

		// Get the standard input handle. 
		if( (stdin_h_ = GetStdHandle(STD_INPUT_HANDLE)) == INVALID_HANDLE_VALUE ) 
			throwx(ex::generic_error("Can't Get StdIn Handle"));
		file_type_ = GetFileType(stdin_h_);

		if( file_type_ == FILE_TYPE_CHAR )
		{
			DebugMessage("InterfaceProcessor Console Mode");

			// Save the current input mode, to be restored on exit. 
			if( !GetConsoleMode(stdin_h_, &old_con_mode_) ) 
				throwx(ex::generic_error("Can't Get Console Mode"));
			// Enable the window and mouse input events.  
			DWORD new_mode = ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT; 
			if( !SetConsoleMode(stdin_h_, new_mode) ) 
				throwx(ex::generic_error("Unable To SetConsoleMode")); 
		}
		else if( file_type_ == FILE_TYPE_PIPE )
		{
			DebugMessage("InterfaceProcessor Pipe Mode");
		}
	}

	string InterfaceProcessor::ID() const
	{
		return Formatter() << "InterfaceProcessor << " << this;
	}

	void InterfaceProcessor::Teardown()
	{
		if( stdin_h_ != INVALID_HANDLE_VALUE && file_type_ == FILE_TYPE_CHAR )
			SetConsoleMode(stdin_h_, old_con_mode_);
		DebugMessage("InterfaceProcessor Teardown");
		server_queue_->push(unique_ptr<msg::ThreadDead>(new msg::ThreadDead(ID())));
	}

	struct PipeInst : public OVERLAPPED
	{
		HANDLE					pipe_;
		InterfaceProcessor*		ifc_proc_;
		class PipeProcessor*	pipe_proc_;
		static const size_t		buf_sz_ = 4096;
		char					buf_[buf_sz_];

	};


	class PipeProcessor 
	{
	public:
		PipeProcessor(HANDLE pipe, std::shared_ptr<msg::MsgQueue> server_queue, InterfaceProcessor::OOBQueue monitor_queue)
			:	pipe_h_(pipe),
			server_queue_(server_queue),
			monitor_queue_(monitor_queue)
		{
		}
		PipeProcessor(PipeProcessor&& rhs)
			:	pipe_h_(move(rhs.pipe_h_)),
			server_queue_(move(rhs.server_queue_)),
			monitor_queue_(move(rhs.monitor_queue_)),
			state_(run_state)
		{
		}

		std::string ID() const
		{
			return Formatter() << "PipeProcessor " << this;
		}

		virtual ~PipeProcessor() {};
		void operator()() ;

	private:
		void DebugMessage(const string& msg) const
		{
			server_queue_->push(unique_ptr<msg::DebugMessage>(new msg::DebugMessage(msg)));
		}

		//static void CompletedPipeReadRoutine(DWORD err, DWORD bytes, OVERLAPPED* ovr);

		enum ThreadState { run_state, die_state}		state_;

		HANDLE pipe_h_;
		std::shared_ptr<msg::MsgQueue>					server_queue_;
		InterfaceProcessor::OOBQueue					monitor_queue_;	
	};

	void PipeProcessor::operator()()
	{
		DebugMessage("PipeProcessor Start");

		using dibcore::util::Formatter;

		DWORD f=0, o_buf=0, i_buf=0, inst=0;
		state_ = run_state;
		while( state_ == run_state )
		{
			DebugMessage("PipeProcessor Loop");
			char buf[256] = {};
			DWORD bytes = 0;
			if( !ReadFile(pipe_h_, buf, sizeof(buf)/sizeof(buf[0]), &bytes, 0) )
			{
				DWORD err = GetLastError();
				if( err == ERROR_OPERATION_ABORTED )
				{
					DebugMessage(dibcore::util::formatwinerr("ReadFile",err));
					server_queue_->push(unique_ptr<msg::ThreadDead>(new msg::ThreadDead(ID())));
					DebugMessage(Formatter()<<ID()<< " Thread Aborted");
					state_ = die_state;
					continue;
				}
				else
				{
					server_queue_->push(unique_ptr<msg::LogMessage>(new msg::LogMessage(Formatter()<<"Error In ReadFile: " << err)));
					monitor_queue_->push(unique_ptr<msg::ThreadDie>(new msg::ThreadDie()));
					continue;
				}
			}

			string cmd(buf,bytes);
			DebugMessage(Formatter()<<"Recv " << bytes << " bytes: '" << cmd << "'");
			monitor_queue_->push(unique_ptr<msg::InternalCommand>(new msg::InternalCommand(cmd)));
		}

		DebugMessage("PipeProcessor Exit");
	}


	void InterfaceProcessor::operator()() 
	{
		Init();

		try
		{
			if( file_type_ == FILE_TYPE_CHAR )
			{
				HANDLE h[] = {oob_queue_->get_native_handle(), stdin_h_};
				DWORD h_n = sizeof(h)/sizeof(h[0]);

				DWORD hb_timeout = 1000;

				while( state_ != die_state )
				{
					DWORD rc = WaitForMultipleObjects(h_n, h, 0, hb_timeout);
					switch( rc )
					{
					case WAIT_OBJECT_0 :		// OOB Message Event
						ProcessOOBEvent();
						break;

					case WAIT_OBJECT_0 + 1 :	// Interface Event
						ProcessInterfaceEvent();
						break;

					case WAIT_TIMEOUT :			// HB timeout
						ProcessHeartBeat();
						break;
					}

				}
			}
			else if( file_type_ == FILE_TYPE_PIPE )
			{
				DebugMessage("InterfaceProcessor Pipe Mode Start");
				Thread<PipeProcessor> pipe_thread(unique_ptr<PipeProcessor>(new PipeProcessor(stdin_h_, this->server_queue_, this->oob_queue_)));
				HANDLE pipe_thread_h = pipe_thread.thread_->native_handle();
				while( state_ != die_state )
				{
					DebugMessage("InterfaceProcessor Loop");

					static const DWORD hb_timeout = 1000;
					DWORD rc = WaitForSingleObject(oob_queue_->get_native_handle(), hb_timeout);
					switch( rc )
					{
					case WAIT_OBJECT_0 :		// OOB Message Event
						DebugMessage("InterfaceProcessor OOB");
						ProcessOOBEvent();
						break;

					case WAIT_TIMEOUT :			// HB timeout
						DebugMessage("InterfaceProcessor HB");
						ProcessHeartBeat();
						break;

					default :
						DebugMessage(Formatter() << "InterfaceProcessor Unexpected Wait Result: " << rc);
					}		
				}

				DebugMessage("InterfaceProcessor Loop End");
				//CancelSynchronousIo(pipe_thread_h);

				//pipe_thread.join();
				DebugMessage("InterfaceProcessor Exit");
			}
		}
		catch(std::exception& ex)
		{
			server_queue_->push(unique_ptr<msg::LogMessage>(new msg::LogMessage(ex.what())));
			Teardown();
			throw;
		}
		Teardown();
	}
};