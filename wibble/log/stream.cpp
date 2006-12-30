#include <wibble/log/stream.h>
#include <ostream>

namespace wibble {
namespace log {

Streambuf::Streambuf() : level(defaultLevel), sender(0) {}

Streambuf::Streambuf(Sender* s) : level(defaultLevel), sender(s) {}

Streambuf::~Streambuf()
{
	if (!line.empty())
		send();
}

void Streambuf::setSender(Sender* s) { sender = s; }

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

std::ostream& operator<<(std::ostream& s, Level lev)
{
	if (Streambuf* ls = dynamic_cast<Streambuf*>(s.rdbuf()))
	{
		ls->setLevel(lev);
		return s;
	} else
		return s << lev;
}

}
}

// vim:set ts=4 sw=4:
