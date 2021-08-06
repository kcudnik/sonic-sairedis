AM_CXXFLAGS = -I../.. -I$(top_srcdir) -I$(top_srcdir)/lib/inc -I$(top_srcdir)/SAI/inc -I$(top_srcdir)/SAI/meta -I$(top_srcdir)/SAI/experimental

lib_LTLIBRARIES = libsairedis.la

noinst_LIBRARIES = libSaiRedis.a
libSaiRedis_a_SOURCES = \
						 ../../syncd/SelectableChannel.cpp \
						 Context.cpp \
						 Notification.cpp \
						 NotificationPortStateChange.cpp \
						 SaiInterface.cpp \
						 VirtualObjectIdManager.cpp \
						 Sai.cpp;

libsairedis_la_SOURCES = \
						 sai_redis_interfacequery.cpp

libSaiRedis_a_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libSaiRedis_a_CXXFLAGS = $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) -std=c++14 $(CODE_COVERAGE_CXXFLAGS)

libsairedis_la_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libsairedis_la_CXXFLAGS = $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) -std=c++14 $(CODE_COVERAGE_CXXFLAGS)
libsairedis_la_LIBADD = libSaiRedis.a $(CODE_COVERAGE_LIBS)


bin_PROGRAMS = tests

tests_SOURCES = tests.cpp
tests_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON)
tests_LDADD = -lpthread $(top_srcdir)/meta/libsaimetadata.la $(top_srcdir)/meta/libsaimeta.la libsairedis.la

TESTS = tests

