#ifndef WIBBLE_COMMANDLINE_CORE_H
#define WIBBLE_COMMANDLINE_CORE_H

#include <wibble/exception.h>
#include <string>
#include <list>

namespace wibble {

namespace exception {
class BadOption : public Consistency
{
	std::string m_error;

public:
	BadOption(const std::string& error, const std::string& context = std::string("parsing commandline options")) throw ()
		: Consistency(context), m_error(error) {}
	~BadOption() throw () {}

	virtual const char* type() const throw () { return "BadOption"; }
	virtual std::string desc() const throw () { return m_error; }

};
}

namespace commandline {
typedef std::list<const char*> arglist;
typedef arglist::iterator iter;
}

}

// vim:set ts=4 sw=4:
#endif
