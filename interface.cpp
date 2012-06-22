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
	state_(run_state)
{

};

InterfaceProcessor::InterfaceProcessor(InterfaceProcessor&& rhs)
:	server_queue_(std::move(rhs.server_queue_)),
	oob_queue_(std::move(rhs.oob_queue_)),
	state_(std::move(rhs.state_))
{
}

void InterfaceProcessor::ProcessOOBEvent()
{
	///*** Get any OOB Messages First ***///
	while( state_ != die_state && !oob_queue_->empty() )
	{
		std::unique_ptr<msg::BasicMessage> oob;
		oob_queue_->wait_and_pop(oob);
		oob->Handle(this);
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
		switch( ir.Event.KeyEvent.wVirtualKeyCode )
		{
		case 'Q' :	// quit command
			server_queue_->push(unique_ptr<msg::ThreadDie>(new msg::ThreadDie));
			break;

		case 'S' :	// stats command
			server_queue_->push(unique_ptr<msg::RequestProgress>(new msg::RequestProgress(msg::RequestProgress::indiv_progress)));
			break;

		case 'P' :	// Pause
			server_queue_->push(unique_ptr<msg::TogglePause>(new msg::TogglePause));
			break;

		}
	}
}

void InterfaceProcessor::HandleThreadDie(msg::ThreadDie*)
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
	// Save the current input mode, to be restored on exit. 
 	if( !GetConsoleMode(stdin_h_, &old_con_mode_) ) 
		throwx(ex::generic_error("Can't Get Console Mode"));
	// Enable the window and mouse input events.  
	DWORD new_mode = ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT; 
	if( !SetConsoleMode(stdin_h_, new_mode) ) 
		throwx(ex::generic_error("Unable To SetConsoleMode")); 
}


void InterfaceProcessor::Teardown()
{
	if( stdin_h_ != INVALID_HANDLE_VALUE )
		SetConsoleMode(stdin_h_, old_con_mode_);
}

void InterfaceProcessor::operator()() 
{
	try
	{
		Init();

		HANDLE h[] = {oob_queue_->get_native_handle(), stdin_h_};
		DWORD h_n = sizeof(h)/sizeof(h[0]);

		while( state_ != die_state )
		{
			DWORD rc = WaitForMultipleObjects(h_n, h, 0, INFINITE);
			switch( rc )
			{
			case WAIT_OBJECT_0 :		// OOB Message Event
				ProcessOOBEvent();
				break;

			case WAIT_OBJECT_0 + 1 :	// Interface Event
				ProcessInterfaceEvent();
				break;
			}
		
		}
	}
	catch(dibcore::ex::generic_error& ex)
	{
		cerr << ex.what();
		Teardown();
		throw;
	}

	Teardown();

}
