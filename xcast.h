#ifndef XCAST_XCAST_H
#define XCAST_XCAST_H

#include <cstdlib>
#include <string>

namespace xcast
{
	struct PacketTime
	{
		PacketTime() : y_(0), m_(0), d_(0), hh_(0), mm_(0), ss_(0), ms_(0) {};
		bool operator<(const PacketTime& rhs) const;
		int	y_, m_, d_, hh_, mm_, ss_, ms_;
		std::string format() const;
	};

}

#include "options.h"


#endif