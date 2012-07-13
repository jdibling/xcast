#ifndef XCAST_OPTIONS_H
#define XCAST_OPTIONS_H

#include <cstdlib>
#include <string>
#include <algorithm>
#include <functional>
#include <vector>
#include "utils.h"
#include "xcast.h"

namespace opts
{
	struct ChannelDesc
	{
		std::string name_;
		std::string file_;
		std::string group_;
		std::string port_;
	};
	typedef std::vector<ChannelDesc> ChannelDescs;
	typedef std::vector<xcast::PacketTime> PacketTimes;

	struct Options
	{
		ChannelDescs	channels_;
		PacketTimes		pauses_;
		unsigned		ttl_;
		unsigned		delay_;
		bool			verb_prog_;
		Options() : ttl_(0), delay_(0), verb_prog_(false) {};
	};

	Options parse_command_line(int ac, char* av[]);

};


#endif