SUBDIRS = . tests
wibbledir = $(includedir)/wibble
wibblecommandlinedir = $(includedir)/wibble/commandline

wibble_HEADERS = \
    amorph.h \
    cast.h \
    config.h \
    consumer.h \
    exception.h \
    fallback.h \
    iterator.h \
    maybe.h \
    mixin.h \
    operators.h \
    range.h \
    shared.h \
    tests.h

wibblecommandline_HEADERS = \
	commandline/core.h \
	commandline/options.h \
	commandline/engine.h \
	commandline/parser.h \
	commandline/doc.h

lib_LTLIBRARIES = libwibble.la
libwibble_la_SOURCES = \
	exception.cpp \
	tests.cpp \
	commandline/core.cpp \
	commandline/options.cpp \
	commandline/engine.cpp \
	commandline/parser.cpp \
	commandline/doc.cpp

#libwibble_la_LIBADD = 
#libwibble_la_LDFLAGS = -no-undefined

INCLUDES = -I$(top_srcdir)/tests/ -I$(top_srcdir) -DWIBBLE_COMPILE_TESTSUITE -DTEST_DIR=\"`pwd`/$(top_builddir)/wibble/tests/work/\"

# vim:set ts=4 sw=4:
