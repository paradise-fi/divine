#include <wibble/config.h>

#ifndef WIBBLE_LOGSTREAM_H
#define WIBBLE_LOGSTREAM_H

#include <streambuf>
#include <string>

// Dummy impl / debug headers
#include <iostream>

namespace wibble {
namespace log {

/// Urgency of a log message
enum Level
{
	DEBUG,
	INFO,
	UNUSUAL,
	WARN,
	ERR,
	CRIT
};

struct Sender
{
	virtual ~Sender() {}
	virtual void send(Level level, const std::string& msg) = 0;
};

class Streambuf : public std::streambuf
{
protected:
	/// Level to use for messages whose level has not been specified
	static const Level defaultLevel = INFO;
	/// Line buffer with the log message we are building
	std::string line;
	/// Level of the next log message
	Level level;

	/// Sender used to send log messages
	/* Note: we have to use composition instead of overloading because the
	 * sender needs to be called in the destructor, and destructors cannot call
	 * overridden methods */
	Sender* sender;

	/// Send the message in line with the level in level
	void send();

public:
	/**
	 * @param s
	 *   The sender to use to send log messages.  Streambuf will just use the
	 *   pointer, but will  not take over memory maintenance
	 */
	Streambuf(Sender* s);
	virtual ~Streambuf();

	/// Set the level for the next message, and the next message only
	void setLevel(const Level& level);

	/// override to get data as a std::streambuf
	int overflow(int c);
};

Streambuf::Streambuf(Sender* s) : level(defaultLevel), sender(s) {}

Streambuf::~Streambuf()
{
	if (!line.empty())
		send();
}

void Streambuf::send()
{
	// Send the message
	sender->send(level, line);
	// Reset the level
	level = defaultLevel;
	line.clear();
}

void Streambuf::setLevel(const Level& level)
{
	this->level = level;
}

int Streambuf::overflow(int c)
{
	if (c == '\n')
		send();
	else
		line += c;
	return c;	// or EOF
}

std::ostream& levWarning(std::ostream &s)
{
	if (Streambuf* ls = dynamic_cast<Streambuf*>(s.rdbuf()))
		ls->setLevel(WARN);
	return s;
}

struct TestSender : public Sender
{
	virtual ~TestSender() {}

	virtual void send(Level level, const std::string& msg)
	{
		std::cerr << level << " -> " << msg << " <-" << std::endl;
	}
};

}
}

#endif

using namespace std;

#include <wibble/tests/tut-wibble.h>

namespace tut {

struct logstream_shar {};
TESTGRP( logstream );

// Instantiate a Streambuf and write something in it
template<> template<>
void to::test< 1 >() {
	using namespace wibble;

	log::TestSender s;
	log::Streambuf ls(&s);
	ostream o(&ls);

	o << "test" << endl;
	o << log::levWarning << "test" << endl;
	o << "should eventually appear";
}

}

// vim:set ts=4 sw=4:
