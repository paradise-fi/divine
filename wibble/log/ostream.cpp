#include <wibble/sys/macros.h>

#include <wibble/log/ostream.h>
#ifdef POSIX
#include <time.h>
#endif
#ifdef _WIN32
#include <ctime>
#endif

namespace wibble {
namespace log {

OstreamSender::OstreamSender(std::ostream& out) : out(out) {}

void OstreamSender::send(Level level, const std::string& msg)
{
	time_t now = time(NULL);
#ifdef POSIX
	struct tm pnow;
	localtime_r(&now, &pnow);
#endif

#ifdef _WIN32
	struct tm * pnow;
	pnow = localtime(&now);
#endif
	char timebuf[20];
	/*
	 * Strftime specifiers used here:
	 *	%b      The abbreviated month name according to the current locale.
	 *	%d      The day of the month as a decimal number (range 01 to 31).
	 *	%e      Like %d, the day of the month as a decimal number, but a
	 *			leading zero is replaced by a space. (SU)
	 *	%T      The time in 24-hour notation (%H:%M:%S). (SU)
	 */
#ifdef POSIX
	strftime(timebuf, 20, "%b %e %T", &pnow);
	out << timebuf << ": " << msg << std::endl;
#endif

#ifdef _WIN32
	out << asctime(pnow) << ": " << msg << std::endl;
#endif
	if (level >= WARN)
		out.flush();
}
	
}
}

// vim:set ts=4 sw=4:
