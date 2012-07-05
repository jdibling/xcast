#include <core/core.h>
#include "utils.h"
using utils::Formatter;
#include "options.h"
using namespace opts;

#include <cstdlib>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
using std::vector;
using std::string;
using std::ifstream;
using std::stringstream;
#include <iostream>
using std::cerr;
using std::cout;
#include <iomanip>
using std::endl;
using std::right;
using std::setw;
using std::setfill;
#include <regex>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string.hpp>
using namespace boost;
using namespace boost::program_options;
namespace fs = boost::filesystem;
using namespace boost::algorithm;

#include "xcast.h"

namespace 
{
	struct OptName
	{
		const string	name_s_;
		const string	abbv_s_;
		const string	fmt_s_;
		const string	desc_s_;
		const char		*name_,
						*abbv_,
						*fmt_,
						*desc_;
		OptName(const std::string& name, const std::string& abbv, const string& desc) 
		:	name_s_(name), 
			abbv_s_(abbv), 
			fmt_s_(make_format()), 
			desc_s_(desc),
			name_(name_s_.c_str()),
			abbv_(abbv_s_.c_str()),
			fmt_(fmt_s_.c_str()),
			desc_(desc_s_.c_str())
		{
		}
		OptName(const std::string& name, const string& desc) 
		:	name_s_(name), 
			abbv_s_(), 
			fmt_s_(make_format()), 
			desc_s_(desc),
			name_(name_s_.c_str()),
			abbv_(abbv_s_.c_str()),
			fmt_(fmt_s_.c_str()),
			desc_(desc_s_.c_str())
		{
		}
	private:
		string make_format() const 
		{
			string ret = name_s_;
			if( !abbv_s_.empty() )
			{
				ret += ",";
				ret += abbv_s_;
			}
			return ret;
		}
	private:
	};
	namespace Base
	{
		static const OptName
			/***	Base Options	***/
			Help		= OptName("help","?", "Display Help"),
			Version		= OptName("version","Show Version Information"),
			Show		= OptName("show","Show Defined Groups (Do Not Send)")
			;
	};
	namespace ChanFile
	{
		static const OptName
			/***	Channel File Options	***/
			DefFile		= OptName("channel-group-defenition-file","f","Channel Definition File CVS Format (name,cap,group,port)")
			;
	};

	namespace PauseFile
	{
		static const OptName
			/***	PAUSE FILE OPTIONS	***/
			DefFile		= OptName("channel-group-pause-file","F","Channel Auto-Pause File TXT format (HHMMSS)")
			;
	};


	namespace ChanDef
	{
		static const OptName
			/***	INLINE CHANNEL DEFINITIONS	***/
			Name		= OptName("channel-name","n","Channel Name"),
			Group		= OptName("group","g","MultiCast Group"),
			Port		= OptName("port","p","MultiCast Port")
			;

		namespace ChanInput
		{
			static const OptName
				/***	INPUT OPTIONS	***/
				CapFile		= OptName("cap-file","c","SpryWare Capture File")
				;
		};
	};

	namespace MCast
	{
		static const OptName
			/***	GENERAL MULTICAST OPTIONS	***/
			TTL			= OptName("ttl","MultiCast Time To Live")
			;
	};

};

Options opts::parse_command_line(int ac, char* av[])
{
	//string
	//	channel_name,
	//	cap_file,
	//	group,
	//	def_file,
	//	pause_file;
	//unsigned 
	//	port = 0,
	//	ttl = 0;
	//
	
	Options ret;
	bool abort = false;

	options_description help_options("Allowed options");

	/***	PROCESS BASE OPTIONS	***/
	options_description base_options("Base Options");
	base_options.add_options()
		(Base::Help.fmt_,		Base::Help.desc_)
		(Base::Version.fmt_,	Base::Version.desc_)
		(Base::Show.fmt_,		Base::Show.desc_)													
		;
	
	help_options.add(base_options);

	enum Mode 
	{
		RunMode,	
		HelpMode,
		ShowMode
	};		// display help banners
	int mode = (int)RunMode;
	variables_map base_vm;
	store(command_line_parser(ac,av).options(base_options).allow_unregistered().run(), base_vm);
	notify(base_vm);

	/***	PROCESS CHANNEL DEFINITION FILE	***/
	if( !abort && (mode==RunMode || mode==ShowMode) )
	{
		// Pull the options
		string channel_file;

		options_description chf_o("Channel File Options");
		chf_o.add_options()
			(ChanFile::DefFile.fmt_,			value<string>(&channel_file),	ChanFile::DefFile.desc_)
		;

		help_options.add(chf_o);

		variables_map chf_vm;
		store(command_line_parser(ac,av).options(chf_o).allow_unregistered().run(), chf_vm);
		notify(chf_vm);

		// Grab the whole file as a single string
		ifstream file(channel_file);
		stringstream buffer;
		buffer << file.rdbuf();
		string buf_str = buffer.str();

		// split the input in to lines			
		typedef char_separator<char> cs;
		tokenizer<cs> lines(buf_str.begin(), buf_str.end(), cs("\n"));
		for( tokenizer<cs>::const_iterator it = lines.begin(), end = lines.end(); it != end; ++it )
		{
			using boost::trim_copy;
			string line(trim_copy(*it));
			if( line.empty() )
				continue;

			tokenizer<cs> t(line.begin(), line.end(), cs(","));
			vector<string> toks(t.begin(), t.end());

			if( toks.size() == 4 )
			{
				ChannelDesc desc;
				desc.name_ = trim_copy(toks[0]);
				desc.file_ = trim_copy(toks[1]);
				desc.group_ = trim_copy(toks[2]);
				desc.port_ = lexical_cast<unsigned>(trim_copy(toks[3]));
				ret.channels_.push_back(desc);
			}
			else if( toks.size() == 3 )
			{
				ChannelDesc desc;
				desc.file_ = trim_copy(toks[0]);
				desc.group_ = trim_copy(toks[1]);
				desc.port_ = lexical_cast<unsigned>(trim_copy(toks[2]));
				ret.channels_.push_back(desc);
			}
			else
			{
				cerr << "Malformed Channel Definition String: '" << line << "' -- Ignored" << endl;
				abort = true;
			}
		}
	}

	/***	PROCESS PAUSE FILE OPTIONS	***/
	string pause_file;
	// Parse the command line options
	options_description pf_o("Pause File Options");
	pf_o.add_options()
		(PauseFile::DefFile.fmt_,	value<string>(&pause_file),		PauseFile::DefFile.desc_)
	;

	help_options.add(pf_o);

	if( !abort && (mode==RunMode||mode==ShowMode) )
	{
		variables_map pf_vm;
		store(command_line_parser(ac,av).options(pf_o).allow_unregistered().run(), pf_vm);
		notify(pf_vm);

		// Grab the whole file in to a single string
		ifstream file(pause_file);
		stringstream buffer;
		buffer << file.rdbuf();
		string buf_str = buffer.str();

		// split the input in to lines			
		typedef char_separator<char> cs;
		tokenizer<cs> lines(buf_str.begin(), buf_str.end(), cs("\n"));
		for( tokenizer<cs>::const_iterator it = lines.begin(), end = lines.end(); it != end; ++it )
		{
			using boost::trim_copy;
			string line(trim_copy(*it));
			if( line.empty() )
				continue;

			const size_t 
				Y = 1,
				M = 2,
				D = 3,
				HH = 4,
				MM = 5,
				SS = 6,
				MS = 7;
							
			std::regex rx("^(?:(\\d{4})\\/(\\d{2})\\/(\\d{2})\\s?)?(\\d{2}):(\\d{2}):?(\\d{2})?\\.?(\\d{2})?");
			std::cmatch rm;
			if( std::regex_search(line.c_str(), rm, rx) )
			{
				xcast::PacketTime pt;
				if( rm[Y].matched )
					pt.m_ = boost::lexical_cast<int>(rm[Y]);
				if( rm[M].matched )
					pt.y_ = boost::lexical_cast<int>(rm[M]);
				if( rm[D].matched )
					pt.d_ = boost::lexical_cast<int>(rm[D]);
				if( rm[HH].matched )
					pt.hh_ = boost::lexical_cast<int>(rm[HH]);
				if( rm[MM].matched )
					pt.mm_ = boost::lexical_cast<int>(rm[MM]);
				if( rm[SS].matched )
					pt.ss_ = boost::lexical_cast<int>(rm[SS]);
				if( rm[MS].matched )
					pt.ms_ = boost::lexical_cast<int>(rm[MS]);
					
				ret.pauses_.push_back(pt);
			}
			else
			{
				cerr << "Malformed Pause String:\n\t Line = '" << line << "'\n\tFile = '" << pause_file << "'\n\t" << endl;
				abort = true;
			}
		}
	}
	
	/***	PROCESS CHANNEL DEFINITION OPTIONS	***/
	ChannelDesc ch;
	options_description chd_o("Channel Definition Options");
	chd_o.add_options()
		(ChanDef::ChanInput::CapFile.fmt_,  value<string>(&ch.file_)->required(),	"Capture File")
		(ChanDef::Name.fmt_,				value<string>(&ch.name_),													ChanDef::Name.desc_)
		(ChanDef::Group.fmt_,				value<string>(&ch.group_)->required(),												ChanDef::Group.desc_)
		(ChanDef::Port.fmt_,				value<string>(&ch.port_)->required(),												ChanDef::Port.desc_)
	;
	help_options.add(chd_o);

	positional_options_description chd_p;
	chd_p.add(ChanDef::Name.name_,1);
	chd_p.add(ChanDef::Group.name_,1);
	chd_p.add(ChanDef::Port.name_,1);
	chd_p.add(ChanDef::ChanInput::CapFile.name_,1);

	if( !abort && (mode==RunMode||mode==ShowMode) )
	{
		variables_map chd_vm;
		store(command_line_parser(ac,av).options(chd_o).allow_unregistered().positional(chd_p).run(), chd_vm);
		bool any_present = 
			(!ch.file_.empty())		||
			(!ch.group_.empty())	||
			(!ch.name_.empty())		||
			(!ch.port_.empty())		;

		if( any_present )
		{
			notify(chd_vm);
			ret.channels_.push_back(ch);
		}
	}

	/***	PROCESS MULTICAST OPTIONS	***/
	options_description mcast_o("MultiCast Options");
	mcast_o.add_options()
		(MCast::TTL.fmt_,	value<unsigned>(&ret.ttl_),	MCast::TTL.desc_)
	;
	help_options.add(mcast_o);
	if( !abort && (mode==RunMode||mode==ShowMode) )
	{
		variables_map mcast_vm;
		store(command_line_parser(ac,av).options(mcast_o).allow_unregistered().run(), mcast_vm);
		notify(mcast_vm);
	}

	/***	HANDLE ERRORS, SHOW HELP SCREEN ***/
	if( abort )
	{
		mode = HelpMode;
		cerr << "Command Syntax Error." << endl;
	}

	if( mode == HelpMode )
	{
		cerr << help_options << endl;
		throwx(dibcore::ex::silent_exception());
	}

	if( mode == ShowMode )
	{
		cerr << "*** CONFIGURED PARAMETERS ***" << endl;
		throwx(dibcore::ex::silent_exception());
	}


	return ret;
}





