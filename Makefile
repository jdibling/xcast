CXX_SOURCES := $(wildcard *.cpp)

OBJECTS := $(patsubst %.cpp, %.o, $(CXX_SOURCES))

SW_LIBS := libMisApi.so

CXXFLAGS = -I /dev/ \
	-I /usr/local/include/ \
	-I ~/dev/SpryWare/MISApi/include/ \
	-I ~/dev/ \
	-std=c++0x \
	-Dlinux
	
all : xcast

clean :
	rm -f $(OBJECTS)

xcast : $(OBJECTS)
	c++ $(CXXFLAGS) -o $@ $(OBJECTS)

.cpp.o :
	$(CC) $(CXXFLAGS) $< -o $@


