#ifndef XCAST_XCAST_H
#define XCAST_XCAST_H

#include <cstdlib>
#include <string>

namespace xcast
{
	struct PacketTime
	{
		PacketTime() : y_(-1), m_(-1), d_(-1), hh_(-1), mm_(-1), ss_(-1), ms_(-1) {};
		bool operator<(const PacketTime& rhs) const;
		int	y_, m_, d_, hh_, mm_, ss_, ms_;
		std::string format() const;
	};

}

#include "options.h"


#endif