CXX_SOURCES := $(wildcard *.cpp)

OBJECTS := $(patsubst %.cpp, %.o, $(CXX_SOURCES))

SW_LIBS := libMisApi.so

CXXFLAGS = -I /dev/ \
	-I /usr/local/include/ \
	-I ~/dev/SpryWare/MISApi/include/ \
	-I ~/dev/ \
	-std=c++0x \
	-Dlinux \
	-lboost_system \
	-lboost_filesystem \
	-lboost_thread \
	-lboost_program_options \
	-lboost_regex \
	-lMisApi \
	-L/usr/local/lib
	
all : xcast

clean :
	rm -f $(OBJECTS)

xcast : $(OBJECTS)
	ctags $(CXX_SOURCES)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJECTS)
	




