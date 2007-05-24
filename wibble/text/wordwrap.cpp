#include <wibble/text/wordwrap.h>
#include <cstdlib>

using namespace std;

namespace wibble {
namespace text {

string WordWrap::get(unsigned int width)
{
	if (cursor >= s.size())
		return "";

	// Find the last work break before `width'
	unsigned int brk = cursor;
	for (unsigned int j = cursor; j < s.size() && j < cursor + width; j++)
	{
		if (s[j] == '\n')
		{
			brk = j;
			break;
		} else if (!isspace(s[j]) && (j + 1 == s.size() || isspace(s[j + 1])))
			brk = j + 1;
	}
	if (brk == cursor)
		brk = cursor + width;

	string res;
	if (brk >= s.size())
	{
		res = string(s, cursor, string::npos);
		cursor = s.size();
	} else {
		res = string(s, cursor, brk - cursor);
		cursor = brk;
		while (cursor < s.size() && isspace(s[cursor]))
			cursor++;
	}
	return res;
}

}
}


// vim:set ts=4 sw=4:
