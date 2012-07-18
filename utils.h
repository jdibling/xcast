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

	const unsigned 
		ver_major = 0,
		ver_minor = 4,
		ver_build = 6;

	inline std::string ver_string()
	{
		return Formatter() << "xcast v. " << ver_major << "." << ver_minor << " (Build " << ver_build << ")";
	}
}

#endif
