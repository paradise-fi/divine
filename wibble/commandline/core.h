#ifndef WIBBLE_COMMANDLINE_CORE_H
#define WIBBLE_COMMANDLINE_CORE_H

#include <wibble/exception.h>
#include <string>
#include <list>

namespace wibble {

namespace exception {
class BadOption : public Consistency
{
public:
	BadOption(const std::string& context) throw ()
		: Consistency(context) {}

	virtual const char* type() const throw () { return "BadOption"; }
};
}

namespace commandline {
typedef std::list<const char*> arglist;
typedef arglist::iterator iter;
}

}

// vim:set ts=4 sw=4:
#endif
