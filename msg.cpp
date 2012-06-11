#include "msg.h"
using namespace msg;

///***  BASIC MESSAGE ***///
BasicMessage::~BasicMessage()
{
}

///*** MESSAGE HANDLER DISPATCHERS ***///
void ThreadDie::Handle(MessageHandler* handler) 
{
	handler->HandleThreadDie(this);
}

void Progress::Handle(MessageHandler* handler) 
{
	handler->HandleProgress(this);
}

void PausePlayback::Handle(MessageHandler* handler) 
{
	handler->HandlePausePlayback(this);
}

void ResumePlayback::Handle(MessageHandler* handler) 
{
	handler->HandleResumePlayback(this);
}

void RequestProgress::Handle(MessageHandler* handler) 
{
	handler->HandleRequestProgress(this);
}

