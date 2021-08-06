AM_CFLAGS = -I$(top_srcdir)/SAI/inc -I$(top_srcdir)/SAI/meta -I$(top_srcdir)/SAI/experimental
AM_CXXFLAGS = -I$(top_srcdir)/SAI/inc -I$(top_srcdir)/SAI/meta -I$(top_srcdir)/SAI/experimental -I$(top_srcdir)/lib/inc

lib_LTLIBRARIES = libsaimeta.la libsaimetadata.la

libsaimeta_la_SOURCES = \
					  $(top_srcdir)/SAI/meta/saimetadata.c \
					  $(top_srcdir)/SAI/meta/saimetadatautils.c \
					  $(top_srcdir)/SAI/meta/saiserialize.c

libsaimeta_la_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libsaimeta_la_CFLAGS = $(AM_CFLAGS) $(CODE_COVERAGE_CFLAGS)

$(top_srcdir)/SAI/meta/saimetadata.h $(top_srcdir)/SAI/meta/saimetadata.c: saimeta-gen

saimeta-gen:
	make -C $(top_srcdir)/SAI/meta

clean-local:
	make -C $(top_srcdir)/SAI/meta clean

libsaimetadata_la_SOURCES = \
							../lib/src/SaiInterface.cpp \
							SaiAttributeList.cpp \
							saiserialize.cpp \
							Globals.cpp


BUILT_SOURCES = $(top_srcdir)/SAI/meta/saimetadata.h $(top_srcdir)/SAI/meta/saimetadata.c

libsaimetadata_la_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libsaimetadata_la_CXXFLAGS = $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)
libsaimetadata_la_LIBADD = libsaimeta.la $(CODE_COVERAGE_LIBS)

bin_PROGRAMS = tests

tests_SOURCES = \
				tests.cpp \
				NumberOidIndexGenerator.cpp \
				../lib/src/VirtualObjectIdManager.cpp

tests_CXXFLAGS = $(AM_CXXFLAGS) $(CXXFLAGS_COMMON)
tests_LDADD = -lpthread libsaimetadata.la libsaimeta.la

TESTS = tests
