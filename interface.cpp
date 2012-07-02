#include "interface.h"
#include "msg.h"
#include <windows.h>
using std::shared_ptr;
#include <core/core.h>
using namespace dibcore;

#include <iostream>
#include <memory>
using namespace std;

InterfaceProcessor::InterfaceProcessor(std::shared_ptr<msg::MsgQueue> server_queue) 
:	server_queue_(server_queue), 
	oob_queue_(shared_ptr<msg::win32::SignalingMsgQueue>(new msg::win32::SignalingMsgQueue)),
	state_(run_state),
	stdin_file_type_(0)
{

};

InterfaceProcessor::InterfaceProcessor(InterfaceProcessor&& rhs)
:	server_queue_(std::move(rhs.server_queue_)),
	oob_queue_(std::move(rhs.oob_queue_)),
	state_(std::move(rhs.state_)),
	stdin_file_type_(rhs.stdin_file_type_)
{
}

void InterfaceProcessor::ProcessOOBEvent()
{
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
	if( stdin_file_type_ == FILE_TYPE_CHAR )
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
 
			case MENU_EVENT:   // disregard menu events 
				break; 
 
			default: 
				throwx(ex::generic_error("Unhandled Input Event"));
				break; 
			}
		}
	}
}

void InterfaceProcessor::ProcessInterfaceEvent_KeyPress(const INPUT_RECORD& ir)
{
	if( !ir.Event.KeyEvent.bKeyDown )
	{
		switch( ir.Event.KeyEvent.wVirtualKeyCode )
		{
		case 'Q' :	// quit command
			server_queue_->push(unique_ptr<msg::ThreadDie>(new msg::ThreadDie));
			break;

		case 'S' :	// stats command
			server_queue_->push(unique_ptr<msg::RequestProgress>(new msg::RequestProgress(msg::RequestProgress::indiv_progress)));
			server_queue_->push(unique_ptr<msg::RequestProgress>(new msg::RequestProgress(msg::RequestProgress::total_progress)));
			break;

		case 'P' :	// Pause
			server_queue_->push(unique_ptr<msg::TogglePause>(new msg::TogglePause));
			break;

		}
	}
}

void InterfaceProcessor::ProcessHeartBeat()
{
	cout << "Pushing HB" << endl;
	server_queue_->push(unique_ptr<msg::HeartBeat>(new msg::HeartBeat()));
}

void InterfaceProcessor::HandleThreadDie(const msg::ThreadDie&)
{
	state_ = die_state;
}

void InterfaceProcessor::Init()
{
	using namespace dibcore;

//	DWORD cNumRead, fdwMode, i; 
//	INPUT_RECORD irInBuf[128]; 
//	int counter=0;
 
	// Get the standard input handle. 
	if( (stdin_h_ = GetStdHandle(STD_INPUT_HANDLE)) == INVALID_HANDLE_VALUE ) 
		throwx(ex::generic_error("Can't Get StdIn Handle"));	
	stdin_file_type_ = GetFileType(stdin_h_);
	cout << "StdIn File Type = X" << stdin_file_type_ << "x" << endl;

	if( stdin_file_type_ == FILE_TYPE_CHAR )
	{
		// Save the current input mode, to be restored on exit. 
		if( !GetConsoleMode(stdin_h_, &old_con_mode_) ) 
		{
			cerr << "Can't get console mode..." << endl;
			throwx(ex::windows_error(GetLastError(), "Can't Get Console Mode"));
		}
		// Enable the window and mouse input events.  
		DWORD new_mode = ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT; 
		if( !SetConsoleMode(stdin_h_, new_mode) ) 
			throwx(ex::generic_error("Unable To SetConsoleMode")); 
	}
}

void InterfaceProcessor::Teardown()
{
	if( stdin_h_ != INVALID_HANDLE_VALUE && stdin_file_type_ == FILE_TYPE_CHAR )
		SetConsoleMode(stdin_h_, old_con_mode_);
}

void InterfaceProcessor::operator()() 
{
	try
	{
		Init();

		if( stdin_file_type_ == FILE_TYPE_CHAR )
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
					cout << "ZIP" << endl;
					break;

				case WAIT_TIMEOUT :			// HB timeout
					ProcessHeartBeat();
					break;
				}
		
			}
		}
		else if( stdin_file_type_ == FILE_TYPE_PIPE )
		{
			char buf[256] = {};
			DWORD bytes = 0;
			if( !ReadFile(stdin_h_, buf, sizeof(buf), &bytes, 0) )
			{
				throwx(dibcore::ex::windows_error(GetLastError(), "ReadFile"));
			}
		
			string s(buf, bytes);
			cout << "Pipe In: '" << s << "'" << endl;
		}
	}
	catch(std::exception& ex)
	{
		cout << "Exception in Interface" << ex.what() << endl;
		cerr << ex.what();
		Teardown();
		return;
	}

	Teardown();

}
