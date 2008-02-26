#include <wibble/sys/fs.h>
#include <wibble/sys/process.h>
#include <wibble/string.h>
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

std::string abspath(const std::string& pathname)
{
	if (pathname[0] == '/')
		return str::normpath(pathname);
	else
		return str::normpath(str::joinpath(process::getcwd(), pathname));
}

void mkdirIfMissing(const std::string& dir, mode_t mode)
{
	std::auto_ptr<struct stat> st = wibble::sys::fs::stat(dir);
	if (st.get() == NULL)
	{
		// If it does not exist, make it
		if (::mkdir(dir.c_str(), mode) == -1)
			throw wibble::exception::System("creating directory " + dir);
	} else if (! S_ISDIR(st->st_mode)) {
		// If it exists but it is not a directory, complain
		throw wibble::exception::Consistency("ensuring path " + dir + " exists", dir + " exists but it is not a directory");
	}
}

void mkpath(const std::string& dir)
{
	size_t pos = dir.rfind('/');
	if (pos != 0 && pos != std::string::npos)
		// First ensure that the upper path exists
		mkpath(dir.substr(0, pos));
	mkdirIfMissing(dir, 0777);
}

void mkFilePath(const std::string& file)
{
	size_t pos = file.rfind('/');
	if (pos != std::string::npos)
		mkpath(file.substr(0, pos));
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
