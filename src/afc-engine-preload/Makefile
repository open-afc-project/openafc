all: aep.so

CXXFLAGS=-Wall -D_GNU_SOURCE -fPIC -DNO_GOOGLE_LINK -O2 #-g 

aep.so: aep.o
	g++ -rdynamic -shared -o $@ $^ #-lgoogle_cloud_cpp_storage

clean:
	rm -fv *.so *.o

