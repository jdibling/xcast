#include "xcast.h"

#include <sstream>
#include <iomanip>
using namespace std;

bool xcast::PacketTime::operator<(const PacketTime& rhs) const
{
	if( (y_ > -1 && rhs.y_ > -1 ) && (y_ != rhs.y_) )
		return y_ < rhs.y_;
	if( (m_ > -1 && rhs.m_ > -1) && (m_ != rhs.m_) )
		return m_ < rhs.m_;
	if( (d_ > -1 && rhs.d_ > -1) && (d_ != rhs.d_) )		
		return d_ < rhs.d_;
	if( hh_ != rhs.hh_ )	
		return hh_ < rhs.hh_;
	if( mm_ != rhs.mm_ )		
		return mm_ < rhs.mm_;
	if( (ss_ > -1 && rhs.ss_ > -1) && (ss_ != rhs.ss_) )		
		return ss_ < rhs.ss_;					
	if( ms_ > -1 && rhs.ms_ > -1 )
		return ms_ < rhs.ms_;
	return false;
}

std::string xcast::PacketTime::format() const
{
	stringstream ss;

	ss 
		<< setw(4) << setfill('0') << y_ << "/"
		<< setw(2) << setfill('0') << m_ << "/" 
		<< setw(2) << setfill('0') << d_ << " " 
		<< setw(2) << setfill('0') << hh_ << ":" 
		<< setw(2) << setfill('0') << mm_ << ":" 
		<< setw(2) << setfill('0') << ss_ << "." 
		<< setw(3) << setfill('0') << ms_;
	return ss.str();
}


