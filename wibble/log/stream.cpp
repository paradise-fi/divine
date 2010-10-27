#include <wibble/log/stream.h>
#include <ostream>

namespace wibble {
namespace log {

Streambuf::Streambuf() : level(defaultLevel), sender(0) {}

Streambuf::Streambuf(Sender* s) : level(defaultLevel), sender(s) {}

Streambuf::~Streambuf()
{
    send_partial_line();
}

void Streambuf::send_partial_line()
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
	} else {
		switch (lev)
		{
			case DEBUG:   s << "DEBUG: "; break;
			case INFO:    s << "INFO: "; break;
			case UNUSUAL: s << "UNUSUAL: "; break;
			case WARN:    s << "WARN: "; break;
			case ERR:     s << "ERR: "; break;
			case CRIT:    s << "CRITICAL: "; break;
			default:      s << "UNKNOWN: "; break;
		}
		return s;
	}
}

}
}

// vim:set ts=4 sw=4:
