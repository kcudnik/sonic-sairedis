
all: saidiscovery

top_srcdir=..

saidiscovery: saidiscovery.cpp
	g++ -std=c++11 saidiscovery.cpp -o saidiscovery \
			 -Wall -Werror \
		-I$(top_srcdir)/vslib/inc -I$(top_srcdir)/lib/inc -I$(top_srcdir)/SAI/inc -I$(top_srcdir)/SAI/meta -I.. \
		-lhiredis -lswsscommon -lsai -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -lopennsl -ldl

