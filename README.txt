 __   _______           _____ _______    ___   ___  
 \ \ / / ____|   /\    / ____|__   __|  / _ \ / _ \ 
  \ V / |       /  \  | (___    | |    | | | | (_) |
   > <| |      / /\ \  \___ \   | |    | | | |\__, |
  / . \ |____ / ____ \ ____) |  | |    | |_| |  / / 
 /_/ \_\_____/_/    \_\_____/   |_|     \___(_)/_/  
 
 

XCAST README

9/14/2012
version 0.9 rev 1


================================================================================

1.	BASIC FEATURES


XCast is a capture file replayer, with a few distinguishing features.  It replays 
cap files by sending them out via UDP multicast.


1.1 Simultaneous Playback of Multiple Capture Files

XCast can replay more than one cap file at a time.  When it does, XCast always 
broadcasts packets in order, according to the timestamps of the packet in the 
capture files.  This comparison is done when every packet is sent, accross
all the capture files being replayed.  The packet with the earliest timestamp
is always sent first, regardless of which capture file it's in.  


1.2 Pausing Playback

XCast can be paused, resumed and restarted.  When it is running, you can hit the 
"P" key ont he keyboard to stop playback.  XCast stops reading the cap files and
waits for you to hit "P" again to resume playback.


1.3 Automatically Pausing Playback At Specific Times

XCast can take an "auto-pause" file on startup.  This auto-pause file is a plain 
text file with a list of times, and optionally dates -- one time/date per line, 
formatted like this:

[YYYY/MM/DD] HH:MM[:SS.uuu]

Only HH and MM are required.  The auto-pause file describes timestamps.  These are 
points at which XCast pauses itself.  If the next packet to go out has a timestamp
that is greater than the next auto-pause found in the auto-pause file, XCast 
automatically pauses itself.  It will not resume playback on it's own.  Just hit 
"P" to resume playback when you're ready.


1.4 Guaranteed Repeatability

XCast guarantees that if you run it twice using the same configuaration and the 
same capture files, it will broadcast packets in exactly the same order and 
automatically pause at exactly the same points both times.


================================================================================

2	RUNNING XCAST

XCast is a command-line program.  You can run it in one of two ways.  You can 
either specify all the runtime parameters on the command line directly,
or you can specify input files that describe what capture files to replay
and when to auto-pause.

The first method is simpler, but the only way to replay more than one cap file
or use auto-pauses is to use the second method.

2.1 Simple Command Line Syntax

To use the first method, the command line syntax is:

	xcast (channel-name) (group) (port) (capture-file) [options]
	
Options are described in the section, "2.5 Command Line Options".  
	
The required parameters are:
	
	Parameter		Description
	============	=========================================================
	channel-name	Name of the multicast channel.  This is displayed in some
					progress messages
					
	group			Multicast group to broadcast on
	
	port			Port to broadcast on
	
	capture-file	Capture file to replay


Example:

	xcast TL2P1 239.255.0.1 30001 c:\capture\Canada_TL2P1.cap


2.2 Multi-Channel Command Line Syntax

In order to replay more than one capture file at once, you must
use the "-f" parameter to specify the channel definition file.
You may also specify an autopause file with the "-F" option.

The xcast command line syntax for multi-channel replay is:

	xcast (-f channel-file) [-F autopause-file] [options]
	
Options are described in the section, "2.5 Command Line Options".  

The required & optional parameters are:
	
	Parameter		Description
	============	=========================================================
	channel-file	Filename specifying the channel definition file.  See
					"2.3 Channel Definition File" below for the instructions
					in creating this file.
					
	autopause-file	Filename specifying the autopauses definition file.  See
					"2.4 Auto-Pause File" for instructions
					
					
					
2.3	Channel Definition File

The channel definiion file is a comma-seperated file where each line describes
one channel.  Each line is formatted as follows, with a newline at the end:

	Name,File,Group,Port

Each field is required, and must be seperated by a comma.  The fields are:

	Field			Description
	============	============================================================
	Name			Channel Name.
					Each channel must have a unique name.  This name appears in
					some progress messages, and is used internally.

	File			Capture file to replay on this channel.

	Group			Multicast group to replay this channel on.

	Port			Port to replay this channel on.

An example of a channel definition file that mimics the same behavior as the 
example from section 2.1:

		TL2P1,c:\capture\Canada_TL2P1.cap,239.255.0.1,30001 
		
Here is an example that replays several channels at once:
		
		TL2P1,c:\capture\Canada_TL2P1.cap,239.255.0.1,30001 
		TL2P2,c:\capture\Canada_TL2P2.cap,239.255.0.2,30002
		CL2,c:\capture\Canada_CL2.cap,239.255.0.3,30003
		Alpha,c:\capture\Canada_Alpha.cap,239.255.0.4,30004
		Omega,c:\capture\Canada_Omega.cap,239.255.0.5,30005
		
Note that the channel name, group and port are all unique for each chahnnel.

2.4 Auto-Pause File

The autopause file is a plain text file with one pause on each line.  The pauses are 
processed in the same order they appear in the file.  That is, xcast doesn't sort 
what you enter in the auto-pause file.  For example, if your autopause file 
has pauses at 09:00, then 08:00, then 08:30 and you replay a capture file that spans
one day, xcast will only autopause once, at 09:00.  

The format of each line in the autopause file is:

	[[YYYY]/MM/DD] HH:MM[:SS[.uuu]]

Where:
	
	Field					Description
	=========				=========================================
	YYYY					4-digit year
	MM						2-digit month
	DD						2-digit day
	HH			[REQUIRED]	2-digit hour, in 24-hour format (ie, for 2:00 PM use 14:00, not 02:00)
	MM			[REQUIRED]	2-digit minute
	SS						2-digit second
	uuu						3-digit millisecond
	
You can only specify YYYY if you also specify MM and DD.

Similarly, you can only specify .uuu if you also specify SS.


2.5 Command Line Options

XCast supports many options that can be used to control the output of various
parameters.  By category of functionality:

	2.5.1	MultiCast Options
	
	Option					Description
	===================		====================================================
	--ttl (arg)				Time To Live.  (arg) is required and is defaulted 
							to zero.  
	
							WARNING:  This is a dangerous option.  Do not
							specify it unless you know exactly what you are doing.
							Specifying a TTL of greater than zero could saturate 
							the entire network and cause a network blackout.
							
	--packet-delay (arg)	Delay time between sending packets.  (arg) is required,
							and specifies the delay time in milliseconds.
							
							NOTE:  Specifying --packet-delay can dramatically 
							increase how long it takes to replay a capture file.
							If you are having problems with packets being rejected on
							the recieving side, you should try to increast the size
							of the recieve buffers before specifying a packet delay.
							
	2.5.2	Logging/Interface Options
	
	Option					Description
	===================		====================================================
	--prog-channels-only	Displays progress messages only for channels, not 
							for groups.  (A group is a collection of channels)
							
	--prog-groups-only		This is the default.  Displays progress messages only 
							for groups, and not for individual channels.
							
	--prog-all				Display progress mesasges for both groups and for
							individual channels.
							
	--raw-output			Displays progress reports in a comma-seperated 
							tag/value format, useful when processing the progress
							messages from a 3rd part application, like MisGlider.
							
	2.5.3	Other Options

	Option					Description
	===================		====================================================
	--version				Show xcast's version number	and exit.
	
	--show					Only show what channels and groups would be replayed,
							then exit immediately without replaying.
							
	--help (-?)				Show the help screen, then exit.
							
				
3	RUNNING XCAST


							
							