#include <wibble/sys/macros.h>
#include <wibble/sys/filelock.h>
#include <wibble/log/file.h>
#include <wibble/exception.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace wibble {
namespace log {

FileSender::FileSender(const std::string& filename) : out(-1), name(filename)
{
	out = open(filename.c_str(), O_APPEND | O_CREAT, 0666);
	if (out < 0)
		throw wibble::exception::File(filename, "opening logfile for append");
}

FileSender::~FileSender()
{
	if (out >= 0) close(out);
}

void FileSender::send(Level level, const std::string& msg)
{
	// Write it all in a single write(2) so multiple processes can log to
	// the same file
	sys::fs::FileLock lock(out, F_WRLCK);

	// Write it all out
	size_t done = 0;
	while (done < msg.size())
	{
		ssize_t res = write(out, msg.data() + done, msg.size() - done);
		if (res < 0)
			throw wibble::exception::File(name, "writing to file");
		done += res;
	}
}
	
}
}

// vim:set ts=4 sw=4:
