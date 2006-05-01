TESTS = libwibble-check
check_PROGRAMS = libwibble-test
check_SCRIPTS = libwibble-check
libwibble_test_SOURCES = tut-main.cpp
libwibble_test_LDADD  = -dlpreopen ../libwibble.la -lgc

noinst_PROGRAMS = commandline-demo
commandline_demo_SOURCES = commandline-demo.cpp
commandline_demo_LDADD  = ../libwibble.la -lgc

INCLUDES = -I../..
