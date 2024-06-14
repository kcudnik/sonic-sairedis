AM_CXXFLAGS = $(SAIINC) -I$(top_srcdir)/proxylib -I$(top_srcdir)/lib -I$(top_srcdir)/vslib -I$(top_srcdir)/syncd

if SAIVS
SAILIB=-L$(top_srcdir)/vslib/.libs -lsaivs
else
SAILIB=-lsai
endif

bin_PROGRAMS = dashapp syncdproxy

dashapp_SOURCES = dashapp.cpp

dashapp_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
dashapp_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS) $(CFLAGS_ASAN)
dashapp_LDADD = $(top_srcdir)/proxylib/libSaiProxy.a \
				$(top_srcdir)/syncd/libSyncd.a \
				-L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta \
			  -ldl -lhiredis -lswsscommon $(SAILIB) -lpthread -lzmq $(CODE_COVERAGE_LIBS) $(EXTRA_LIBSAI_LDFLAGS)
dashapp_LDFLAGS = $(LDFLAGS_ASAN) -rdynamic

SAIPROXYLIB=-L$(top_srcdir)/proxylib/.libs -lsaiproxy

syncdproxy_SOURCES = ../syncd/main.cpp
syncdproxy_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
syncdproxy_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS) $(CFLAGS_ASAN)
syncdproxy_LDADD = $(top_srcdir)/syncd/libSyncd.a $(SAIPROXYLIB) $(top_srcdir)/lib/libSaiRedis.a -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta \
	         	  -ldl -lhiredis -lswsscommon -lpthread -lzmq $(CODE_COVERAGE_LIBS) $(EXTRA_LIBSAI_LDFLAGS)
syncdproxy_LDFLAGS = $(LDFLAGS_ASAN) -rdynamic
