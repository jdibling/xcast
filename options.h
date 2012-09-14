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

	typedef enum {normal_fmt, verbose_fmt, raw_fmt} OutputFormat;
	typedef enum {show_channels, show_groups, show_both} ShowType;
	struct Options
	{
		ChannelDescs	channels_;
		PacketTimes		pauses_;
		unsigned		ttl_;
		unsigned		delay_;
		ShowType		show_type_;
		OutputFormat	out_fmt_;
		bool			show_mode_;	// show parameters and exit if true
		Options() : ttl_(0), delay_(0), out_fmt_(normal_fmt), show_type_(show_groups) {};
	};

	Options parse_command_line(int ac, char* av[]);

};


#endif