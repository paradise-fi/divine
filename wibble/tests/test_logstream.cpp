#include <wibble/log/stream.h>
#include <wibble/log/null.h>
#include <wibble/log/file.h>
#include <wibble/config.h>
#include <vector>
#include <iostream>

namespace wibble {
namespace log {

// Test sender for log::Streambuf
struct TestSender : public Sender
{
	// Here go the log messages
	std::vector< std::pair<Level, std::string> > log;

	virtual ~TestSender() {}

	// Interface for the streambuf to send messages
	virtual void send(Level level, const std::string& msg)
	{
		log.push_back(make_pair(level, msg));
	}

	// Dump all the logged messages to cerr
	void dump()
	{
		for (size_t i = 0; i < log.size(); ++i)
			std::cerr << log[i].first << " -> " << log[i].second << " <-" << std::endl;
	}
};

}
}

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
	{
		log::Streambuf ls(&s);
		ostream o(&ls);

		// Send a normal log message
		o << "test" << endl;
		ensure_equals(s.log.size(), 1u);
		ensure_equals(s.log[0].first, log::INFO);
		ensure_equals(s.log[0].second, "test");

		// Send a log message with a different priority
		//o << log::lev(log::WARN) << "test" << endl;
		o << log::WARN << "test" << endl;
		ensure_equals(s.log.size(), 2u);
		ensure_equals(s.log[1].first, log::WARN);
		ensure_equals(s.log[1].second, "test");

		// Ensure that log messages are only sent after a newline
		o << "should eventually appear";
		ensure_equals(s.log.size(), 2u);
	}
	// Or at streambuf destruction
	ensure_equals(s.log.size(), 3u);
	ensure_equals(s.log[2].first, log::INFO);
	ensure_equals(s.log[2].second, "should eventually appear");

	//s.dump();
}

// Test the NullSender
template<> template<>
void to::test< 2 >() {
	using namespace wibble;

	// Null does nothing, so we cannot test the results.

	log::NullSender ns;
	ns.send(log::INFO, "test");

	log::Streambuf null(&ns);
	ostream o(&null);

	// Send a normal log message
	o << "test" << endl;

	// Send a log message with a different priority
	//o << log::lev(log::WARN) << "test" << endl;
	o << log::WARN << "test" << endl;

	// Ensure that log messages are only sent after a newline
	o << "should eventually appear";
}

// Test the FileSender
template<> template<>
void to::test< 3 >() {
	using namespace wibble;

	// Null does nothing, so we cannot test the results.

	log::FileSender ns("/dev/null");
	ns.send(log::INFO, "test");

	log::Streambuf file(&ns);
	ostream o(&file);

	// Send a normal log message
	o << "test" << endl;

	// Send a log message with a different priority
	//o << log::lev(log::WARN) << "test" << endl;
	o << log::WARN << "test" << endl;

	// Ensure that log messages are only sent after a newline
	o << "should eventually appear";
}

}

// vim:set ts=4 sw=4:
