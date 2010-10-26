#include <wibble/log/ostream.h>

namespace wibble {
namespace log {

OstreamSender::OstreamSender(std::ostream& out) : out(out) {}

void OstreamSender::send(Level level, const std::string& msg)
{
	out << msg << std::endl;
	if (level >= WARN)
		out.flush();
}
	
}
}

// vim:set ts=4 sw=4:
