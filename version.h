#define STRINGIZE2(s) #s
#define STRINGIZE(s) STRINGIZE2(s)

#define VERSION_MAJOR               0
#define VERSION_MINOR               9
#define VERSION_REVISION            2
#define VERSION_BUILD               206

#define VER_COMPANYNAME_STR			"SpryWare"
#define VER_FILE_DESCRIPTION_STR    "xcast -- Simultaneous Capture Playback With Guaranteed Packet Order"
#define VER_FILE_VERSION            VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION, VERSION_BUILD

#if defined (VERSION_REVISION) && ( VERSION_REVISION > 0 )

#define VER_FILE_VERSION_STR        STRINGIZE(VERSION_MAJOR)        \
	"." STRINGIZE(VERSION_MINOR)    \
	"r" STRINGIZE(VERSION_REVISION) \
	" (Build " STRINGIZE(VERSION_BUILD)    \
	")" \

#else

#define VER_FILE_VERSION_STR        STRINGIZE(VERSION_MAJOR)        \
	"." STRINGIZE(VERSION_MINOR)    \
	" (Build " STRINGIZE(VERSION_BUILD)    \
	")" \

#endif


#define VER_PRODUCTNAME_STR         "xcast"
#define VER_PRODUCT_VERSION         VER_FILE_VERSION
#define VER_PRODUCT_VERSION_STR     VER_FILE_VERSION_STR
#define VER_ORIGINAL_FILENAME_STR   VER_PRODUCTNAME_STR ".exe"
#define VER_INTERNAL_NAME_STR       VER_ORIGINAL_FILENAME_STR
#define VER_COPYRIGHT_STR           "Copyright (C) 2012, SpryWare"

#ifdef _DEBUG
#define VER_VER_DEBUG             VS_FF_DEBUG
#else
#define VER_VER_DEBUG             0
#endif

#define VER_FILEOS                  VOS_NT_WINDOWS32
#define VER_FILEFLAGS               VER_VER_DEBUG
#define VER_FILETYPE                VFT_APP

