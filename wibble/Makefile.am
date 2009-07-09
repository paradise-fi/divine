SUBDIRS = .

INCLUDES = -I$(top_srcdir)

if WIBBLE_STANDALONE
INCLUDES += -DWIBBLE_COMPILE_TESTSUITE -DTEST_DIR=\"`pwd`/$(top_builddir)/wibble/tests/work/\"

wibbledir = $(includedir)/wibble
wibblecommandlinedir = $(includedir)/wibble/commandline
wibblelogdir = $(includedir)/wibble/log
wibblesysdir = $(includedir)/wibble/sys
wibbletestdir = $(includedir)/wibble/tests
wibbletextdir = $(includedir)/wibble/text
wibblegrcaldir = $(includedir)/wibble/grcal
wibblestreamdir = $(includedir)/wibble/stream

wibble_HEADERS = \
    amorph.h \
    cast.h \
    consumer.h \
    empty.h \
    exception.h \
    exception.tcc \
    fallback.h \
    iterator.h \
    maybe.h \
    mixin.h \
    operators.h \
    range.h \
    regexp.h \
    sfinae.h \
    singleton.h \
    string.h \
    tests.h

wibblesys_HEADERS = \
    sys/thread.h \
    sys/process.h \
    sys/netbuffer.h \
    sys/mutex.h \
    sys/fs.test.h \
    sys/buffer.h \
    sys/mmap.h \
    sys/childprocess.h \
    sys/exec.h \
    sys/fs.h \
    sys/signal.h \
    sys/lockfile.h

wibblelog_HEADERS = \
    log/stream.h \
    log/null.h \
    log/syslog.h \
    log/ostream.h \
    log/file.h

wibblecommandline_HEADERS = \
    commandline/core.h \
    commandline/doc.h \
    commandline/engine.h \
    commandline/options.h \
    commandline/parser.h

wibbletest_HEADERS = \
    tests/tut-prereq.h \
    tests/tut_reporter.h \
    tests/tut-wibble.h \
    tests/tut.h \
    tests/tut_restartable.h

wibbletext_HEADERS = \
    text/wordwrap.h

wibblegrcal_HEADERS = \
    grcal/grcal.h

wibblestream_HEADERS = \
    stream/posix.h

lib_LTLIBRARIES = libwibble.la

else
noinst_LTLIBRARIES = libwibble.la
endif

libwibble_la_SOURCES = \
    exception.cpp \
    regexp.cpp \
    string.cpp \
    test.cpp \
    tests.cpp \
    sys/fs.cpp \
    sys/lockfile.cpp \
    sys/buffer.cpp \
    sys/mmap.cpp \
    sys/process.cpp \
    sys/childprocess.cpp \
    sys/mutex.cpp \
    sys/exec.cpp \
    sys/thread.cpp \
    sys/signal.cpp \
    log/file.cpp \
    log/stream.cpp \
    log/ostream.cpp \
    log/syslog.cpp \
    commandline/core.cpp \
    commandline/options.cpp \
    commandline/doc.cpp \
    commandline/parser.cpp \
    commandline/engine.cpp \
    text/wordwrap.cpp \
    grcal/grcal.cpp

#libwibble_la_LIBADD = 
#libwibble_la_LDFLAGS = -no-undefined

EXTRA_DIST = README.repo libwibble.m4 libwibble.pc.in \
    amorph.test.h \
    consumer.test.h \
    empty.test.h \
    exception.test.h \
    log.test.h \
    mixin.test.h \
    operators.test.h \
    range.test.h \
    regexp.test.h \
    singleton.test.h \
    string.test.h \
    commandline/core.test.h \
    commandline/options.test.h \
    commandline/doc.test.h \
    commandline/engine.test.h \
    test.h \
    test-main.h \
    test-runner.h \
    test.test.h \
    sys/childprocess.test.h \
    sys/netbuffer.test.h \
    sys/buffer.test.h \
    sys/mmap.test.h \
    sys/process.test.h \
    sys/thread.test.h \
    sys/fs.test.h \
    sys/lockfile.test.h \
    sys/signal.test.h \
    grcal/grcal.test.h \
    stream/posix.test.h

# vim:set ts=4 sw=4:
