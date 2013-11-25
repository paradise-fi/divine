#ifndef WIBBLE_SYS_DIRECTORY_H
#define WIBBLE_SYS_DIRECTORY_H

#include <string>
#include <memory>       // auto_ptr
#include <sys/types.h>  // mode_t
#include <sys/stat.h>   // struct stat
#include <unistd.h>     // access

struct dirent;

namespace wibble {
namespace sys {
namespace fs {

/**
 * stat() the given file and return the struct stat with the results.
 * If the file does not exist, return NULL.
 * Raises exceptions in case of errors.
 */
std::auto_ptr<struct stat64> stat(const std::string& pathname);

/// access() a filename
bool access(const std::string& s, int m);

/// Same as access(s, F_OK);
bool exists(const std::string& s);

/**
 * Get the absolute path of a file
 */
std::string abspath(const std::string& pathname);

// Create a temporary directory based on a template.
std::string mkdtemp( std::string templ );

/// Create the given directory, if it does not already exists.
/// It will complain if the given pathname already exists but is not a
/// directory.
void mkdirIfMissing(const std::string& dir, mode_t mode = 0777);

/// Create all the component of the given directory, including the directory
/// itself.
void mkpath(const std::string& dir);

/// Ensure that the path to the given file exists, creating it if it does not.
/// The file itself will not get created.
void mkFilePath(const std::string& file);

/// Read whole file into memory. Throws exceptions on failure.
std::string readFile(const std::string &file);

/// Read whole file into memory. Throws exceptions on failure.
std::string readFile(std::ifstream &file);

/// Write \a data to \a file, replacing existing contents if it already exists
void writeFile(const std::string &file, const std::string &data);

/**
 * Delete a file if it exists. If it does not exist, do nothing.
 *
 * @return true if the file was deleted, false if it did not exist
 */
bool deleteIfExists(const std::string& file);

/// Move \a src to \a dst, without raising exception if \a src does not exist
void renameIfExists(const std::string& src, const std::string& dst);

/// Delete the file
void unlink(const std::string& fname);

/// Remove the directory using rmdir(2)
void rmdir(const std::string& dirname);

/// Delete the directory \a dir and all its content
void rmtree(const std::string& dir);

/**
 * Returns true if the given pathname is a directory, else false.
 *
 * It also returns false if the pathname does not exist.
 */
bool isdir(const std::string& pathname);

/// Same as isdir but checks for block devices
bool isblk(const std::string& pathname);

/// Same as isdir but checks for character devices
bool ischr(const std::string& pathname);

/// Same as isdir but checks for FIFOs
bool isfifo(const std::string& pathname);

/// Same as isdir but checks for symbolic links
bool islnk(const std::string& pathname);

/// Same as isdir but checks for regular files
bool isreg(const std::string& pathname);

/// Same as isdir but checks for sockets
bool issock(const std::string& pathname);


/// Nicely wrap access to directories
class Directory
{
protected:
    std::string m_path;
    // DIR* pointer
    void* dir;

public:
    class const_iterator
    {
        Directory* dir;
        struct dirent* d;

    public:
        // Create an end iterator
        const_iterator() : dir(0), d(0) {}
        // Create a begin iterator
        const_iterator(Directory& dir) : dir(&dir), d(0) { ++(*this); }
        // Cleanup properly
        ~const_iterator();

		// auto_ptr style copy semantics
		const_iterator(const const_iterator& i)
		{
			dir = i.dir;
			d = i.d;
			const_iterator* wi = const_cast<const_iterator*>(&i);
			wi->dir = 0;
			wi->d = 0;
		}
        const_iterator& operator=(const const_iterator& i);

        const_iterator& operator++();

        std::string operator*() const;

		bool operator==(const const_iterator& iter) const
		{
			return dir == iter.dir && d == iter.d;
		}
		bool operator!=(const const_iterator& iter) const
		{
			return dir != iter.dir || d != iter.d;
		}

        /// @return true if we refer to a directory, else false
        bool isdir() const;

        /// @return true if we refer to a block device, else false
        bool isblk() const;

        /// @return true if we refer to a character device, else false
        bool ischr() const;

        /// @return true if we refer to a named pipe (FIFO).
        bool isfifo() const;

        /// @return true if we refer to a symbolic link.
        bool islnk() const;

        /// @return true if we refer to a regular file.
        bool isreg() const;

        /// @return true if we refer to a Unix domain socket.
        bool issock() const;
    };

    Directory(const std::string& path);
    ~Directory();

	/// Pathname of the directory
	const std::string& path() const { return m_path; }

	/// Begin iterator
	const_iterator begin();

	/// End iterator
	const_iterator end() const;
};

}
}
}

// vim:set ts=4 sw=4:
#endif
