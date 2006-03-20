wibbledir = $(includedir)/wibble
wibblecommandlinedir = $(includedir)/wibble/commandline

wibble_HEADERS = \
	exception.h \
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

INCLUDES = -I$(top_srcdir)/tests/ -I$(top_srcdir) -DCOMPILE_TESTSUITE -DTEST_DIR=\"`pwd`/$(top_builddir)/tests/data/\"

# vim:set ts=4 sw=4:
