wibbledir = $(includedir)/wibble

wibble_HEADERS = tests.h

lib_LTLIBRARIES = libwibble.la
libwibble_la_SOURCES = tests.cpp

#libwibble_la_LIBADD = 
#libwibble_la_LDFLAGS = -no-undefined

INCLUDES = -I$(top_srcdir)/tests/ -I$(top_srcdir) -DCOMPILE_TESTSUITE -DTEST_DIR=\"`pwd`/$(top_builddir)/tests/data/\"

# vim:set ts=4 sw=4:
