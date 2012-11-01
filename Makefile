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
	-L/usr/local/lib
	
all : xcast

clean :
	rm -f $(OBJECTS)

xcast : $(OBJECTS)
	echo ==SOURCES==
	echo $(CXX_COURCES)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJECTS)



