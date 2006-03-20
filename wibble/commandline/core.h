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

class ArgList : public std::list<std::string>
{
public:
	// Remove the item pointed by the iterator, and advance the iterator to the
	// next item.  Returns i itself.
	inline iterator& eraseAndAdvance(iterator& i)
	{
		if (i == end())
			return i;
		iterator next = i;
		++next;
		erase(i);
		i = next;
		return i;
	}

	static bool isSwitch(const char* str);
	static bool isSwitch(const std::string& str);
	static bool isSwitch(const const_iterator& iter);
	static bool isSwitch(const iterator& iter);
};

}

}

// vim:set ts=4 sw=4:
#endif
