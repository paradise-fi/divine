#include <wibble/log/syslog.h>

#ifdef POSIX
namespace wibble {
namespace log {

SyslogSender::SyslogSender(const std::string& ident, int option, int facility)
{
	openlog(ident.c_str(), option, facility);
}

SyslogSender::~SyslogSender()
{
	closelog();
}

void SyslogSender::send(Level level, const std::string& msg)
{
	int prio = LOG_INFO;
	switch (level)
	{
		case DEBUG: prio = LOG_DEBUG; break;
		case INFO: prio = LOG_INFO; break;
		case UNUSUAL: prio = LOG_NOTICE; break;
		case WARN: prio = LOG_WARNING; break;
		case ERR: prio = LOG_ERR; break;
		case CRIT: prio = LOG_CRIT; break;
		//LOG_ALERT, LOG_EMERG
	}
	syslog(prio, "%s", msg.c_str());
}

}
}
#endif
// vim:set ts=4 sw=4:
