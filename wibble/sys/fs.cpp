#include <wibble/sys/fs.h>
#include <wibble/sys/process.h>
#include <wibble/string.h>
#include <wibble/exception.h>
#include <fstream>
#include <sys/stat.h>
#include <errno.h>
#include <malloc.h> // alloca on win32 seems to live there

namespace wibble {
namespace sys {
namespace fs {

#ifdef POSIX
std::auto_ptr<struct stat> stat(const std::string& pathname)
{
	std::auto_ptr<struct stat> res(new struct stat);
	if (::stat(pathname.c_str(), res.get()) == -1) {
		if (errno == ENOENT)
			return std::auto_ptr<struct stat>();
		else
			throw wibble::exception::System("getting file information for " + pathname);
        }
	return res;
}

bool isDirectory(const std::string& pathname)
{
	struct stat st;
	if (::stat(pathname.c_str(), &st) == -1) {
		if (errno == ENOENT)
			return false;
		else
			throw wibble::exception::System("getting file information for " + pathname);
        }
	return S_ISDIR(st.st_mode);
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
#endif
std::string readFile( const std::string &file )
{
    std::ifstream in( file.c_str(), std::ios::binary );
    if (!in.is_open())
        throw wibble::exception::System( "reading file " + file );
    std::string ret;
    size_t length;

    in.seekg(0, std::ios::end);
    length = in.tellg();
    in.seekg(0, std::ios::beg);
    char *buffer = (char *) alloca( length );

    in.read(buffer, length);
    return std::string( buffer, length );
}

void writeFile( const std::string &file, const std::string &data )
{
    std::ofstream out( file.c_str(), std::ios::binary );
    if (!out.is_open())
        throw wibble::exception::System( "writing file " + file );
    out << data;
}

bool deleteIfExists(const std::string& file)
{
	if (unlink(file.c_str()) != 0)
		if (errno != ENOENT)
			throw wibble::exception::File(file, "removing file");
		else
			return false;
	else
		return true;
}

#ifdef POSIX
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
#endif

#ifdef _WIN32
bool access(const std::string &s, int m)
{
	return 1; /* FIXME */
}
#endif

}
}
}

// vim:set ts=4 sw=4:
