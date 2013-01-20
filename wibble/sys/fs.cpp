#include <wibble/sys/fs.h>
#include <wibble/sys/process.h>
#include <wibble/string.h>
#include <wibble/exception.h>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <malloc.h> // alloca on win32 seems to live there

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#include <tchar.h>
#include <shellapi.h>
#endif

#ifdef HAVE_CONFIG_H
/* Conditionally include config.h so there is a way of enabling the fast
 * Directory::isdir implementation if HAVE_STRUCT_DIRENT_D_TYPE is available
 */
#include <wibble/config.h>
#endif

namespace wibble {
namespace sys {
namespace fs {

#ifdef POSIX
std::unique_ptr<struct stat> stat(const std::string& pathname)
{
	std::unique_ptr<struct stat> res(new struct stat);
	if (::stat(pathname.c_str(), res.get()) == -1) {
		if (errno == ENOENT)
			return std::unique_ptr<struct stat>();
		else
			throw wibble::exception::System("getting file information for " + pathname);
        }
	return res;
}

bool isDirectory(const std::string& pathname)
{
    return isdir(pathname);
}

bool isdir(const std::string& pathname)
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

bool exists(const std::string& file)
{
    return sys::fs::access(file, F_OK);
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
	std::unique_ptr<struct stat> st = wibble::sys::fs::stat(dir);
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
    char *buffer = static_cast<char *>( alloca( length ) );

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
	if (::unlink(file.c_str()) != 0)
		if (errno != ENOENT)
			throw wibble::exception::File(file, "removing file");
		else
			return false;
	else
		return true;
}

void renameIfExists(const std::string& src, const std::string& dst)
{
    int res = ::rename(src.c_str(), dst.c_str());
    if (res < 0 && errno != ENOENT)
        throw wibble::exception::System("moving " + src + " to " + dst);
}

void unlink(const std::string& fname)
{
    if (::unlink(fname.c_str()) < 0)
        throw wibble::exception::File(fname, "cannot delete file");
}

void rmdir(const std::string& dirname)
{
    if (::rmdir(dirname.c_str()) < 0)
        throw wibble::exception::System("cannot delete directory " + dirname);
}

#ifdef POSIX
void rmtree(const std::string& dir)
{
    Directory d(dir);
    for (Directory::const_iterator i = d.begin(); i != d.end(); ++i)
    {
        if (*i == "." || *i == "..") continue;
        if (d.isdir(i))
            rmtree(str::joinpath(dir, *i));
        else
            unlink(str::joinpath(dir, *i));
    }
    rmdir(dir);
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
	std::unique_ptr<struct stat> st = stat(path());
	if (st.get() == NULL)
		return false;
	// Check that it is a directory
	if (!S_ISDIR(st->st_mode))
		return false;
	return true;
}

bool Directory::isdir(const const_iterator& i) const
{
#ifdef HAVE_STRUCT_DIRENT_D_TYPE
	if (i->d_type == DT_DIR)
		return true;
	if (i->d_type != DT_UNKNOWN)
		return false;
#endif
	// No d_type, we'll need to stat
	std::unique_ptr<struct stat> st = stat(wibble::str::joinpath(m_path, *i));
	if (st.get() == 0)
		return false;
	if (S_ISDIR(st->st_mode))
		return true;
    return false;
}

std::string mkdtemp( std::string tmpl )
{
    char *_tmpl = reinterpret_cast< char * >( alloca( tmpl.size() + 1 ) );
    strcpy( _tmpl, tmpl.c_str() );
    return ::mkdtemp( _tmpl );
}

#endif

#ifdef _WIN32
bool access(const std::string &s, int m)
{
	return 1; /* FIXME */
}

std::string mkdtemp( std::string tmpl )
{
    char *_tmpl = reinterpret_cast< char * >( alloca( tmpl.size() + 1 ) );
    strcpy( _tmpl, tmpl.c_str() );

    if ( _mktemp_s( _tmpl, tmpl.size() + 1 ) == 0 ) {
        if ( ::mkdir( _tmpl ) == 0 )
            return _tmpl;
        else
            throw wibble::exception::System("creating temporary directory");
    } else {
        throw wibble::exception::System("creating temporary directory path");
    }
}
// Use strictly ANSI variant of structures and functions.
void rmtree( const std::string& dir ) {
    // Use double null terminated path.
    int len = dir.size();
    char* from = reinterpret_cast< char* >( alloca( len + 2 ) );
    strcpy( from, dir.c_str() );
    from [ len + 1 ] = '\0';

    SHFILEOPSTRUCTA fileop;
    fileop.hwnd   = NULL;    // no status display
    fileop.wFunc  = FO_DELETE;  // delete operation
    fileop.pFrom  = from;  // source file name as double null terminated string
    fileop.pTo    = NULL;    // no destination needed
    fileop.fFlags = FOF_NOCONFIRMATION | FOF_SILENT;  // do not prompt the user

    fileop.fAnyOperationsAborted = FALSE;
    fileop.lpszProgressTitle     = NULL;
    fileop.hNameMappings         = NULL;

    int ret = SHFileOperationA( &fileop );
    if ( ret )// only zero return value is without error
        throw wibble::exception::System( "deleting directory" );
}
#endif

}
}
}

// vim:set ts=4 sw=4:
