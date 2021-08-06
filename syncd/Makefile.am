AM_CXXFLAGS = -I$(top_srcdir)/lib/inc -I$(top_srcdir)/vslib/inc -I$(top_srcdir)/SAI/inc -I$(top_srcdir)/SAI/meta -I$(top_srcdir)/SAI/experimental

bin_PROGRAMS = syncd tests

noinst_LIBRARIES = libSyncd.a
libSyncd_a_SOURCES = \
				SaiSwitchInterface.cpp \
				SelectableChannel.cpp \
				syncd_main.cpp \
				CommandLineOptions.cpp 

libSyncd_a_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libSyncd_a_CXXFLAGS = $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(SAIFLAGS) $(CODE_COVERAGE_CXXFLAGS)

syncd_SOURCES = main.cpp
syncd_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
syncd_CXXFLAGS = $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(SAIFLAGS) $(CODE_COVERAGE_CXXFLAGS)
syncd_LDADD = libSyncd.a ../lib/src/libSaiRedis.a -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -ldl  $(SAILIB) -lpthread $(CODE_COVERAGE_LIBS)

tests_SOURCES = tests.cpp

tests_CXXFLAGS = $(AM_CXXFLAGS) $(CXXFLAGS_COMMON)
tests_LDADD = libSyncd.a -lpthread -L$(top_srcdir)/lib/src/.libs -lsairedis -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta $(CODE_COVERAGE_LIBS)

TESTS = tests
