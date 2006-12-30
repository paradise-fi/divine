#include <wibble/log/ostream.h>
#include <time.h>

namespace wibble {
namespace log {

OstreamSender::OstreamSender(std::ostream& out) : out(out) {}

void OstreamSender::send(Level level, const std::string& msg)
{
	time_t now = time(NULL);
	struct tm pnow;
	localtime_r(&now, &pnow);
	char timebuf[20];
	/*
	 * Strftime specifiers used here:
	 *	%b      The abbreviated month name according to the current locale.
	 *	%d      The day of the month as a decimal number (range 01 to 31).
	 *	%e      Like %d, the day of the month as a decimal number, but a
	 *			leading zero is replaced by a space. (SU)
	 *	%T      The time in 24-hour notation (%H:%M:%S). (SU)
	 */
	strftime(timebuf, 20, "%b %e %T", &pnow);
	out << timebuf << ": " << msg << std::endl;
	if (level >= WARN)
		out.flush();
}
	
}
}

// vim:set ts=4 sw=4:
