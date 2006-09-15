#include <wibble/sys/fs.h>
#include <wibble/exception.h>
#include <sys/stat.h>
#include <errno.h>

namespace wibble {
namespace sys {
namespace fs {

std::auto_ptr<struct stat> stat(const std::string& pathname)
{
	std::auto_ptr<struct stat> res(new struct stat);
	if (::stat(pathname.c_str(), res.get()) == -1)
		if (errno == ENOENT)
			return std::auto_ptr<struct stat>();
		else
			throw wibble::exception::System("getting file information for " + pathname);
	return res;
}

bool access(const std::string &s, int m)
{
	return ::access(s.c_str(), m) == 0;
}

Directory::const_iterator Directory::begin()
{
	DIR* dir = opendir(m_path.c_str());
	if (!dir)
		throw wibble::exception::System("reading directory " + m_path);
	return const_iterator(dir);
}

Directory::const_iterator Directory::end() const
{
	return const_iterator();
}

bool Directory::valid()
{
	// Check that the directory exists
	std::auto_ptr<struct stat> st = stat(path());
	if (st.get() == NULL)
		return false;
	// Check that it is a directory
	if (!S_ISDIR(st->st_mode))
		return false;
	return true;
}

}
}
}

// vim:set ts=4 sw=4:
