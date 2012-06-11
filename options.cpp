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
#include <iomanip>
using std::endl;
using std::right;
using std::setw;
using std::setfill;

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


Options opts::parse_command_line(int ac, char* av[])
{
	string
		channel_name,
		cap_file,
		group,
		def_file;
	unsigned 
		port = 0;
	
	Options ret;
	options_description options("Allowed options");
	options.add_options()
		("help,?",																										"Display Help")
		("version",																										"Show Version Information")
		("channel-name,n",				value<string>(&channel_name),													"Channel Name")
		("cap-file,c",					value<string>(&cap_file)->required(),											"Capture File")
		("group,g",						value<string>(&group)->required(),												"MultiCast Group")
		("port,p",						value<unsigned>(&port)->required(),												"MultiCast Port")
		("channel-definition-file,f",	value<string>(),																"Channel Definition File CVS Format (name,cap,group,port)")
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
		if( vm.count("channel-definition-file") )
		{
			/* The channel definition file is in CSV format, formatted like this:
			 *
			 *		name,cap-file,group,port
			 */

			// Grab the whole file in to a single string
			def_file = vm["channel-definition-file"].as<string>();

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

				tokenizer<cs> t(line.begin(), line.end(), cs(","));
				vector<string> toks(t.begin(), t.end());

				if( toks.size() == 4 )
				{
					Channel ch;
					ch.name_ = trim_copy(toks[0]);
					ch.file_ = trim_copy(toks[1]);
					ch.group_ = trim_copy(toks[2]);
					ch.port_ = lexical_cast<unsigned>(trim_copy(toks[3]));
					ret.channels_.push_back(ch);
				}
				else if( toks.size() == 3 )
				{
					Channel ch;
					ch.file_ = trim_copy(toks[0]);
					ch.group_ = trim_copy(toks[1]);
					ch.port_ = lexical_cast<unsigned>(trim_copy(toks[2]));
					ret.channels_.push_back(ch);
				}
				else
				{
					cerr << "Malformed Input String: '" << line << "' -- Ignored" << endl;
				}
			}

		}

		if( vm.count("group") )
		{
			try
			{
				notify(vm);			

				Channel channel;

				if( vm.count("channel-name") )
					channel.name_ = channel_name;
				if( vm.count("cap-file") )
					channel.file_ = cap_file;
				if( vm.count("group") )
					channel.group_ = group;
				if( vm.count("port") )
					channel.port_ = port;

				ret.channels_.push_back(channel);
			
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
			Channel& c = ret.channels_[i];
			if( c.name_.empty() )
				c.name_ = Formatter() << "Ch" << setw(2) << setfill('0') << right << i+1;
		}
	}

	if( show_ver )
		cerr << "xcast\t\tver 0.01a\t\tBy John Dibling\n";
	if( show_help )
		cerr << options << endl;
	if( abort )
		throwx(dibcore::ex::silent_exception());

	return ret;
}



