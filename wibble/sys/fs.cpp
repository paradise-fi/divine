#include <wibble/config.h>

#include <wibble/sys/fs.h>
#include <wibble/sys/process.h>
#include <wibble/string.h>
#include <wibble/exception.h>
#include <fstream>
#include <dirent.h> // opendir, closedir
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

namespace wibble {
namespace sys {
namespace fs {

#ifdef POSIX
std::auto_ptr<struct stat64> stat(const std::string& pathname)
{
	std::auto_ptr<struct stat64> res(new struct stat64);
	if (::stat64(pathname.c_str(), res.get()) == -1) {
		if (errno == ENOENT)
			return std::auto_ptr<struct stat64>();
		else
			throw wibble::exception::System("getting file information for " + pathname);
        }
	return res;
}

#define common_stat_body(testfunc) \
    struct stat st; \
    if (::stat(pathname.c_str(), &st) == -1) { \
        if (errno == ENOENT) \
            return false; \
        else \
            throw wibble::exception::System("getting file information for " + pathname); \
    } \
    return testfunc(st.st_mode)

bool isdir(const std::string& pathname)
{
    common_stat_body(S_ISDIR);
}

bool isblk(const std::string& pathname)
{
    common_stat_body(S_ISBLK);
}

bool ischr(const std::string& pathname)
{
    common_stat_body(S_ISCHR);
}

bool isfifo(const std::string& pathname)
{
    common_stat_body(S_ISFIFO);
}

bool islnk(const std::string& pathname)
{
    common_stat_body(S_ISLNK);
}

bool isreg(const std::string& pathname)
{
    common_stat_body(S_ISREG);
}

bool issock(const std::string& pathname)
{
    common_stat_body(S_ISSOCK);
}

#undef common_stat_body

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
    for (int i = 0; i < 5; ++i)
    {
        // If it does not exist, make it
        if (::mkdir(dir.c_str(), mode) != -1)
            return;

        // throw on all errors except EEXIST. Note that EEXIST "includes the case
        // where pathname is a symbolic link, dangling or not."
        if (errno != EEXIST)
            throw wibble::exception::System("creating directory " + dir);

        // Ensure that, if dir exists, it is a directory
        std::auto_ptr<struct stat64> st = wibble::sys::fs::stat(dir);
        if (st.get() == NULL)
        {
            // Either dir has just been deleted, or we hit a dangling
            // symlink.
            //
            // Retry creating a directory: the more we keep failing, the more
            // the likelyhood of a dangling symlink increases.
            //
            // We could lstat here, but it would add yet another case for a
            // race condition if the broken symlink gets deleted between the
            // stat and the lstat.
            continue;
        }
        else if (! S_ISDIR(st->st_mode))
            // If it exists but it is not a directory, complain
            throw wibble::exception::Consistency("ensuring path " + dir + " exists", dir + " exists but it is not a directory");
        else
            // If it exists and it is a directory, we're fine
            return;
    }
    throw wibble::exception::Consistency("ensuring path " + dir + " exists", dir + " exists and looks like a dangling symlink");
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
#endif // POSIX

std::string readFile( std::ifstream &in ) {
    if (!in.is_open())
        throw wibble::exception::System( "reading filestream" );
    size_t length;

    in.seekg(0, std::ios::end);
    length = in.tellg();
    in.seekg(0, std::ios::beg);

    std::string buffer;
    buffer.resize( length );

    in.read( &buffer[ 0 ], length );
    return buffer;
}

std::string readFile( const std::string &file ) {
    std::ifstream in( file.c_str(), std::ios::binary );
    if (!in.is_open())
        throw wibble::exception::System( "reading file " + file );
    return readFile( in );
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
        if (i.isdir())
            rmtree(str::joinpath(dir, *i));
        else
            unlink(str::joinpath(dir, *i));
    }
    rmdir(dir);
}

Directory::const_iterator::~const_iterator()
{
    if (d) delete d;
}

Directory::const_iterator& Directory::const_iterator::operator=(const Directory::const_iterator& i)
{
    // Catch a = a
    if (&i == this) return *this;
    dir = i.dir;
    d = i.d;
    const_iterator* wi = const_cast<const_iterator*>(&i);
    // Turn i into an end iterator
    wi->dir = 0;
    wi->d = 0;
    return *this;
}

Directory::const_iterator& Directory::const_iterator::operator++()
{
    if (!d) d = new struct dirent;

    struct dirent* dres;
    int res = readdir_r(static_cast<DIR*>(dir->dir), d, &dres);
    if (res != 0)
        throw wibble::exception::System(res, "reading directory " + dir->m_path);

    if (dres == NULL)
    {
        // Turn into an end iterator
        dir = 0;
        delete d;
        d = 0;
    }
    return *this;
}

std::string Directory::const_iterator::operator*() const { return d->d_name; }

bool Directory::const_iterator::isdir() const
{
#ifdef HAVE_STRUCT_DIRENT_D_TYPE
    if (d->d_type == DT_DIR)
        return true;
    if (d->d_type != DT_UNKNOWN)
        return false;
#endif
    // No d_type, we'll need to stat
    return wibble::sys::fs::isdir(wibble::str::joinpath(dir->m_path, d->d_name));
}

bool Directory::const_iterator::isblk() const
{
#ifdef HAVE_STRUCT_DIRENT_D_TYPE
    if (d->d_type == DT_BLK)
        return true;
    if (d->d_type != DT_UNKNOWN)
        return false;
#endif
    // No d_type, we'll need to stat
    return wibble::sys::fs::isblk(wibble::str::joinpath(dir->m_path, d->d_name));
}

bool Directory::const_iterator::ischr() const
{
#ifdef HAVE_STRUCT_DIRENT_D_TYPE
    if (d->d_type == DT_CHR)
        return true;
    if (d->d_type != DT_UNKNOWN)
        return false;
#endif
    // No d_type, we'll need to stat
    return wibble::sys::fs::ischr(wibble::str::joinpath(dir->m_path, d->d_name));
}

bool Directory::const_iterator::isfifo() const
{
#ifdef HAVE_STRUCT_DIRENT_D_TYPE
    if (d->d_type == DT_FIFO)
        return true;
    if (d->d_type != DT_UNKNOWN)
        return false;
#endif
    // No d_type, we'll need to stat
    return wibble::sys::fs::isfifo(wibble::str::joinpath(dir->m_path, d->d_name));
}

bool Directory::const_iterator::islnk() const
{
#ifdef HAVE_STRUCT_DIRENT_D_TYPE
    if (d->d_type == DT_LNK)
        return true;
    if (d->d_type != DT_UNKNOWN)
        return false;
#endif
    // No d_type, we'll need to stat
    return wibble::sys::fs::islnk(wibble::str::joinpath(dir->m_path, d->d_name));
}

bool Directory::const_iterator::isreg() const
{
#ifdef HAVE_STRUCT_DIRENT_D_TYPE
    if (d->d_type == DT_REG)
        return true;
    if (d->d_type != DT_UNKNOWN)
        return false;
#endif
    // No d_type, we'll need to stat
    return wibble::sys::fs::isreg(wibble::str::joinpath(dir->m_path, d->d_name));
}

bool Directory::const_iterator::issock() const
{
#ifdef HAVE_STRUCT_DIRENT_D_TYPE
    if (d->d_type == DT_SOCK)
        return true;
    if (d->d_type != DT_UNKNOWN)
        return false;
#endif
    // No d_type, we'll need to stat
    return wibble::sys::fs::issock(wibble::str::joinpath(dir->m_path, d->d_name));
}


Directory::Directory(const std::string& path)
    : m_path(path), dir(0)
{
    dir = opendir(m_path.c_str());
    if (!dir)
        throw wibble::exception::System("reading directory " + m_path);
}

Directory::~Directory()
{
    if (dir) closedir(static_cast<DIR*>(dir));
}

Directory::const_iterator Directory::begin()
{
    return const_iterator(*this);
}

Directory::const_iterator Directory::end() const
{
    return const_iterator();
}

std::string mkdtemp( std::string tmpl )
{
    char *_tmpl = reinterpret_cast< char * >( alloca( tmpl.size() + 1 ) );
    strcpy( _tmpl, tmpl.c_str() );
    return ::mkdtemp( _tmpl );
}
#endif // POSIX

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
