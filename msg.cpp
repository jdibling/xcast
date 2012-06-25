#include "msg.h"
using namespace msg;

///***  BASIC MESSAGE ***///
BasicMessage::~BasicMessage()
{
}

///*** MESSAGE HANDLER DISPATCHERS ***///
void HeartBeat::Handle(MessageHandler* handler)
{
	handler->HandleHeartBeat(this);
}

void ThreadDie::Handle(MessageHandler* handler) 
{
	handler->HandleThreadDie(this);
}

void ThreadDead::Handle(MessageHandler* handler) 
{
	handler->HandleThreadDead(this);
}

void GroupProgress::Handle(MessageHandler* handler) 
{
	handler->HandleGroupProgressReport(this);
}

void ChannelProgress::Handle(MessageHandler* handler) 
{
	handler->HandleChannelProgressReport(this);
}

void TogglePause::Handle(MessageHandler* handler)
{
	handler->HandleTogglePause(this);
}

void RequestProgress::Handle(MessageHandler* handler) 
{
	handler->HandleRequestProgress(this);
}

