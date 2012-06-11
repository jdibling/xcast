#ifndef XCAST_OPTIONS_H
#define XCAST_OPTIONS_H

#include <cstdlib>
#include <string>
#include <algorithm>
#include <functional>
#include <vector>
#include "utils.h"

namespace opts
{
	struct ChannelDesc
	{
		std::string name_;
		std::string file_;
		std::string group_;
		unsigned port_;
	};
	typedef std::vector<ChannelDesc> ChannelDescs;

	struct Options
	{
		ChannelDescs	channels_;
	};

	Options parse_command_line(int ac, char* av[]);

};


#endif