#include <wibble/config.h>

#ifndef WIBBLE_LOGSTREAM_H
#define WIBBLE_LOGSTREAM_H

#include <streambuf>
#include <string>

// Dummy impl / debug headers
#include <iostream>

namespace wibble {

class LogStream : public std::streambuf
{
protected:
	std::string line;
	std::string level;

	void send();

public:
	LogStream();
	virtual ~LogStream();

	void setLevel(const std::string& level);

	int overflow(int c);
};

LogStream::LogStream() : level("warning") {}

LogStream::~LogStream()
{
	if (!line.empty())
		send();
}

void LogStream::send()
{
	std::cerr << level << " -> " << line << " <-" << std::endl;
	line.clear();
	// Reset the level
	level = "warning";
}

void LogStream::setLevel(const std::string& level)
{
	this->level = level;
}

int LogStream::overflow(int c)
{
	if (c == '\n')
		send();
	else
		line += c;
	return c;	// or EOF
}

std::ostream& levWarning(std::ostream &s)
{
	if (LogStream* ls = dynamic_cast<LogStream*>(s.rdbuf()))
		ls->setLevel("WARNING");
	return s;
}

}

#endif

using namespace std;

#include <wibble/tests/tut-wibble.h>

namespace tut {

struct logstream_shar {};
TESTGRP( logstream );

// Instantiate a LogStream and write something in it
template<> template<>
void to::test< 1 >() {
	wibble::LogStream ls;
	ostream o(&ls);

	o << "test" << endl;
	o << wibble::levWarning << "test" << endl;
	o << "should eventually appear";
}

}

// vim:set ts=4 sw=4:
