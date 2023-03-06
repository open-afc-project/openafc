# Copyright (C) 2023 Broadcom. All rights reserved.
# The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
# that owns the software below.
# This work is licensed under the OpenAFC Project License, a copy of which is
# included with this software program.

all: aep.so

CXXFLAGS=-Wall -D_GNU_SOURCE -fPIC -DNO_GOOGLE_LINK -O2 #-g

aep.so: aep.o
	g++ -rdynamic -shared -o $@ $^ #-lgoogle_cloud_cpp_storage

clean:
	rm -fv *.so *.o

