#include <wibble/sys/macros.h>
#include <wibble/log/file.h>
#include <wibble/exception.h>
#include <stdio.h>
#ifdef POSIX
#include <time.h>
#endif

#ifdef _WIN32
#include <ctime>
#endif

namespace wibble {
namespace log {

FileSender::FileSender(const std::string& filename) : out(0), name(filename)
{
	out = fopen(filename.c_str(), "at");
	if (!out)
		throw wibble::exception::File(filename, "opening logfile for append");
}

FileSender::~FileSender()
{
	if (out) fclose((FILE*)out);
}

void FileSender::send(Level level, const std::string& msg)
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
	fprintf((FILE*)out, "%s: %s\n", timebuf, msg.c_str());
#endif

#ifdef _WIN32
	fprintf((FILE*)out, "%s: %s\n",  asctime(pnow), msg.c_str());
#endif
	if (level >= WARN)
		fflush((FILE*)out);
}
	
}
}

// vim:set ts=4 sw=4:
