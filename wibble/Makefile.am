SUBDIRS = .

INCLUDES = -I$(top_srcdir)

if WIBBLE_STANDALONE
SUBDIRS += tests
INCLUDES += -DWIBBLE_COMPILE_TESTSUITE -DTEST_DIR=\"`pwd`/$(top_builddir)/wibble/tests/work/\"

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
    regexp.h \
    string.h \
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

config.h: config.h.autotools
	sed -e 's/^S:/#define /' < $< > $@

else
noinst_LTLIBRARIES = libwibble.la

config.h: config.h.autotools
	sed -e 's/^S:/#undef /' < $< > $@
endif

BUILT_SOURCES = config.h

libwibble_la_SOURCES = \
    exception.cpp \
    regexp.cpp \
    tests.cpp \
    sys/fs.cpp \
    sys/buffer.cpp \
    sys/process.cpp \
    sys/childprocess.cpp \
    sys/mutex.cpp \
    sys/exec.cpp \
    sys/thread.cpp \
    log/file.cpp \
    log/stream.cpp \
    log/ostream.cpp \
    log/syslog.cpp \
    commandline/core.cpp \
    commandline/options.cpp \
    commandline/doc.cpp \
    commandline/parser.cpp \
    commandline/engine.cpp \
    text/wordwrap.cpp

#libwibble_la_LIBADD = 
#libwibble_la_LDFLAGS = -no-undefined

EXTRA_DIST = libwibble.m4 libwibble.pc.in config.h.autotools config.h.cmake-in

# vim:set ts=4 sw=4:
