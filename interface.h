#ifndef XCAST_INTERFACE
#define XCAST_INTERFACE

#include <cstdlib>
#include <memory>

#include "msg.h"

class InterfaceProcessor 
{
public:
	static std::unique_ptr<InterfaceProcessor> Create(std::shared_ptr<msg::MsgQueue> server_queue);
	virtual ~InterfaceProcessor() = 0;
	virtual void operator()() = 0;

	//virtual void kill() = 0;

	virtual void PushOOBMessage(std::unique_ptr<msg::BasicMessage>&&) = 0;
	
protected:
	InterfaceProcessor(){};
};

#endif
