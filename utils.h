#ifndef XCAST_UTILS_H
#define XCAST_UTILS_H

#include <cstdlib>
#include <string>
#include <sstream>

namespace utils
{
	class Formatter
	{
	public:
		template<class Val> Formatter& operator<<(const Val& val)
		{
			ss_ << val;
			return * this;
		}
		operator std::string () const;
		std::string get() const;
	private:
		std::stringstream ss_;
	};

	std::string as_bytes(__int64 bytes);
}

#endif
