AM_CPPFLAGS = \
			  -I$(top_srcdir)/lib/inc \
			  -I$(top_srcdir)/vslib/inc \
			  -I$(top_srcdir)/SAI/inc \
			  -I$(top_srcdir)/SAI/meta \
			  -I$(top_srcdir)/SAI/experimental \
			  -I$(top_srcdir)/meta \
			  -I$(top_srcdir)/syncd

bin_PROGRAMS = saiasiccmp

if DEBUG
DBGFLAGS = -ggdb -DDEBUG
else
DBGFLAGS = -g
endif

if SAIVS
SAILIB=-L$(top_srcdir)/vslib/src/.libs -lsaivs
else
SAILIB=-lsai
endif

noinst_LIBRARIES = libAsicCmp.a
libAsicCmp_a_SOURCES = \
				AsicCmp.cpp \
				SaiSwitchAsic.cpp

libAsicCmp_a_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON) $(SAIFLAGS) -std=c++14

saiasiccmp_SOURCES = main.cpp
saiasiccmp_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON) $(SAIFLAGS)
saiasiccmp_LDADD = libAsicCmp.a \
				   -lsaimetadata -lsaimeta -ldl -lhiredis -lswsscommon $(SAILIB) -lpthread -lzmq \
				   $(top_srcdir)/syncd/libSyncd.a \
				   -L$(top_srcdir)/syncd/.libs \
				   $(top_srcdir)/lib/src/libSaiRedis.a \
				   -L$(top_srcdir)/meta/.libs
