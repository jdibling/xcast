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
	handler->HandleProgressReport(this);
}

void TogglePause::Handle(MessageHandler* handler)
{
	handler->HandleTogglePause(this);
}

void RequestProgress::Handle(MessageHandler* handler) 
{
	handler->HandleRequestProgress(this);
}

