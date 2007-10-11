SUBDIRS = . tests
wibbledir = $(includedir)/wibble
wibblecommandlinedir = $(includedir)/wibble/commandline
wibbletestdir = $(includedir)/wibble/tests

wibble_HEADERS = \
    amorph.h \
    cast.h \
    config.h \
    consumer.h \
    exception.h \
    exception.tcc \
    fallback.h \
    iterator.h \
    maybe.h \
    mixin.h \
    operators.h \
    empty.h \
    singleton.h \
    range.h \
    shared.h \
    tests.h

wibblecommandline_HEADERS = \
	commandline/core.h \
	commandline/options.h \
	commandline/engine.h \
	commandline/parser.h \
	commandline/doc.h

wibbletest_HEADERS = \
    tests/tut-prereq.h \
    tests/tut-wibble.h

lib_LTLIBRARIES = libwibble.la
libwibble_la_SOURCES = \
	exception.cpp \
	empty.cpp \
	singleton.cpp \
	consumer.cpp \
	mixin.cpp \
	operators.cpp \
	range.cpp \
	tests.cpp \
	commandline/core.cpp \
	commandline/options.cpp \
	commandline/engine.cpp \
	commandline/parser.cpp \
	commandline/doc.cpp

#libwibble_la_LIBADD = 
#libwibble_la_LDFLAGS = -no-undefined

INCLUDES = -I$(top_srcdir)/tests/ -I$(top_srcdir) -DWIBBLE_COMPILE_TESTSUITE -DTEST_DIR=\"`pwd`/$(top_builddir)/wibble/tests/work/\"


pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libwibble.pc libwibble-gc.pc

m4dir = $(datadir)/aclocal
m4_DATA = libwibble.m4

EXTRA_DIST = libwibble.m4 libwibble.pc.in libwibble-gc.pc.in

# vim:set ts=4 sw=4:
