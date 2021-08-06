AM_CXXFLAGS = -I../.. -I$(top_srcdir)/vslib/inc -I$(top_srcdir)/SAI/inc -I$(top_srcdir)/SAI/meta -I/usr/include/libnl3 -I$(top_srcdir)/SAI/experimental -I$(top_srcdir)/lib/inc

lib_LTLIBRARIES = libsaivs.la

noinst_LIBRARIES = libSaiVS.a
libSaiVS_a_SOURCES = \
					  ../../lib/src/Notification.cpp \
					  ../../lib/src/NotificationPortStateChange.cpp \
					  Context.cpp \
					  Event.cpp \
					  Sai.cpp;

libsaivs_la_SOURCES = \
					  sai_vs_interfacequery.cpp

libSaiVS_a_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libSaiVS_a_CXXFLAGS = $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)

libsaivs_la_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libsaivs_la_CXXFLAGS = $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)
libsaivs_la_LIBADD =  libSaiVS.a $(CODE_COVERAGE_LIBS)

bin_PROGRAMS = tests

tests_SOURCES = tests.cpp
tests_CXXFLAGS = $(AM_CXXFLAGS) $(CXXFLAGS_COMMON)
tests_LDADD = -lpthread libsaivs.la $(top_srcdir)/meta/libsaimetadata.la $(top_srcdir)/meta/libsaimeta.la

TESTS = tests
