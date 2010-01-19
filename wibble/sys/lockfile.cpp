#include <wibble/sys/lockfile.h>

#ifdef POSIX

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <wibble/exception.h>

namespace wibble {
namespace sys {
namespace fs {

Lockfile::Lockfile(const std::string& name, bool write) : name(name), fd(-1)
{
	fd = open(name.c_str(), (write ? O_RDWR : O_RDONLY) | O_CREAT, 0666);
	if (fd == -1)
		throw wibble::exception::System("opening/creating lockfile " + name);
	struct flock lock;
	lock.l_type = (write ? F_WRLCK : F_RDLCK);
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;
	if (fcntl(fd, F_SETLK, &lock) == -1)
		throw wibble::exception::System("locking the file " + name + " for writing");
}

Lockfile::~Lockfile()
{
	if (fd != -1)
	{
		struct flock lock;
		lock.l_type = F_UNLCK;
		lock.l_whence = SEEK_SET;
		lock.l_start = 0;
		lock.l_len = 0;
		fcntl(fd, F_SETLK, &lock);
		close(fd);
	}
}

}
}
}

// vim:set ts=4 sw=4:

#endif
