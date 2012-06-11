#include <cstdlib>
#include <string>
using std::string;
#include "utils.h"

/*
 *	FORMATTER 
 */

utils::Formatter::operator string () const 
{ 
	return get(); 
}

string utils::Formatter::get() const 
{ 
	return ss_.str().c_str(); 
}

/*
 *	AS_BYTES
 */

string utils::as_bytes(__int64 bytes)
{
	if( bytes < 1024 )
		return Formatter() << bytes << " B";
	bytes /= 1024;
	if( bytes < 1024 )
		return Formatter() << bytes << " KB";
	bytes /= 1024;
	if( bytes < 1024 )
		return Formatter() << bytes << " MB";
	bytes /= 1024;
	if( bytes < 1024 )
		return Formatter() << bytes << " GB";
	bytes /= 1024;
	return Formatter() << bytes << " TB";
}
