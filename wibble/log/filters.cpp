#include <wibble/log/filters.h>
#include <wibble/sys/macros.h>

#ifdef POSIX
#include <time.h>
#endif

#ifdef _WIN32
#include <ctime>
#endif

using namespace std;

namespace wibble {
namespace log {

Timestamper::Timestamper(Sender* next, const std::string& fmt)
	: next(next), fmt(fmt)
{
}

Timestamper::~Timestamper()
{
}

void Timestamper::send(Level level, const std::string& msg)
{
	if (!next) return;

	time_t now = time(NULL);
#ifdef POSIX
	struct tm pnow;
	localtime_r(&now, &pnow);
#endif

#ifdef _WIN32
	struct tm * pnow;
	pnow = localtime(&now);
#endif
	/*
	 * Strftime specifiers used here:
	 *	%b      The abbreviated month name according to the current locale.
	 *	%d      The day of the month as a decimal number (range 01 to 31).
	 *	%e      Like %d, the day of the month as a decimal number, but a
	 *			leading zero is replaced by a space. (SU)
	 *	%T      The time in 24-hour notation (%H:%M:%S). (SU)
	 */
#ifdef POSIX
	char timebuf[256];
	strftime(timebuf, 256, fmt.c_str(), &pnow);
	next->send(level, string(timebuf) + msg);
#endif

#ifdef _WIN32
	next->send(level, string(asctime(pnow)) + " " + msg);
#endif
}

LevelFilter::LevelFilter(Sender* next, log::Level minLevel)
	: next(next), minLevel(minLevel)
{
}

LevelFilter::~LevelFilter()
{
}

void LevelFilter::send(log::Level level, const std::string& msg)
{
	if (level < minLevel || !next) return;
	next->send(level, msg);
}

Tee::Tee() {}
Tee::Tee(Sender* first, Sender* second)
{
	next.push_back(first);
	next.push_back(second);
}
Tee::~Tee()
{
}

void Tee::send(log::Level level, const std::string& msg)
{
	for (std::vector<Sender*>::iterator i = next.begin(); i != next.end(); ++i)
		(*i)->send(level, msg);
}

}
}

// vim:set ts=4 sw=4:
