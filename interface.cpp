#include "interface.h"

#ifdef _WIN32

#include "win/interface_win.h"

std::unique_ptr<InterfaceProcessor> InterfaceProcessor::Create(std::shared_ptr<msg::MsgQueue> server_queue)
{
	return std::unique_ptr<InterfaceProcessor>(new win::InterfaceProcessor(server_queue));
}

#endif
