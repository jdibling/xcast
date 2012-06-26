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


Options opts::parse_command_line(int ac, char* av[])
{
	string
		channel_name,
		cap_file,
		group,
		def_file,
		pause_file;
	unsigned 
		port = 0;
	
	Options ret;
	options_description options("Allowed options");
	options.add_options()
		("help,?",																											"Display Help")
		("version",																											"Show Version Information")
		("channel-name,n",					value<string>(&channel_name),													"Channel Name")
		("cap-file,c",						value<string>(&cap_file)->required(),											"Capture File")
		("group,g",							value<string>(&group)->required(),												"MultiCast Group")
		("port,p",							value<unsigned>(&port)->required(),												"MultiCast Port")
		("channel-group-definition-file,f",	value<string>(),																"Channel Definition File CVS Format (name,cap,group,port)")
		("channel-group-pause-file,F",		value<string>(),																"Channel Auto-Pause File TXT format (HHMMSS)")
	;

	positional_options_description p;
	p.add("cap-file",1);
	p.add("group",1);
	p.add("port",1);

	variables_map vm;        
	store(command_line_parser(ac,av).options(options).positional(p).run(), vm);
	//store(parse_command_line(ac, av, options), vm);

	enum Mode 
	{
		RunMode,	
		HelpMode
	}		// display help banners
	mode;
	
	mode = RunMode;
	bool show_help = false;
	bool show_ver = false;
	bool abort = false;
	
	if( vm.empty() )
	{
		mode = HelpMode;
		show_help = true;
		show_ver = true;
		abort = true;
	}
	else if( vm.count("help") )
	{
		mode = HelpMode;
		show_help = true;
		abort = true;
	}
	else if( vm.count("version") )
	{
		mode = HelpMode;
		show_ver = true;
		abort = true;
	}

	if( mode != HelpMode )
	{
		if( vm.count("channel-group-definition-file") )
		{
			/* The channel definition file is in CSV format, formatted like this:
			 *
			 *		name,cap-file,group,port
			 */

			// Grab the whole file in to a single string
			pause_file = vm["channel-group-definition-file"].as<string>();
			vector<xcast::PacketTime> pause_times;
			
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

		if( vm.count("channel-group-pause-file") )
		{
			/* The channel definition file is in CSV format, formatted like this:
			 *
			 *		name,cap-file,group,port
			 */

			// Grab the whole file in to a single string
			def_file = vm["channel-group-pause-file"].as<string>();

			ifstream file(def_file);
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
					cerr << "Malformed Pause String: '" << line << "' -- Ignored" << endl;
					abort = true;
				}
			}

		}

		if( vm.count("group") )
		{
			try
			{
				notify(vm);			

				ChannelDesc desc;

				if( vm.count("channel-name") )
					desc.name_ = channel_name;
				if( vm.count("cap-file") )
					desc.file_ = cap_file;
				if( vm.count("group") )
					desc.group_ = group;
				if( vm.count("port") )
					desc.port_ = port;

				ret.channels_.push_back(desc);
			
			}
			catch( const required_option& ex )
			{
				show_help = true;
				cerr << "Missing required Option: " << ex.get_option_name() << endl;
				abort = true;
			}
		}

		// Give each un-named channel a default name
		for( size_t i = 0; i < ret.channels_.size(); ++i )
		{
			ChannelDesc& desc = ret.channels_[i];
			if( desc.name_.empty() )
				desc.name_ = Formatter() << "Ch" << setw(2) << setfill('0') << right << i+1;
		}
	}

	if( abort )
		show_ver = show_help = true;

	if( show_ver )
		cerr << "xcast\t\tver 0.01a\t\tBy John Dibling\n";
	if( show_help )
		cerr << options << endl;
	if( abort )
		throwx(dibcore::ex::silent_exception());

	return ret;
}



