#include <wibble/sys/filelock.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <wibble/exception.h>

#ifdef POSIX

namespace wibble {
namespace sys {
namespace fs {

FileLock::FileLock(int fd, short l_type, short l_whence, off64_t l_start, off64_t l_len)
    : fd(fd)
{
    lock.l_type = l_type;
    lock.l_whence = l_whence;
    lock.l_start = l_start;
    lock.l_len = l_len;
    if (fcntl(fd, F_SETLKW64, &lock) == -1)
        throw wibble::exception::System("locking file");
}

FileLock::~FileLock()
{
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK64, &lock);
}

}
}
}

#endif

// vim:set ts=4 sw=4:
