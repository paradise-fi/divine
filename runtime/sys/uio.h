#ifndef _SYS_UIO_H
#define _SYS_UIO_H  1

#include <sys/types.h>


/* `struct iovec' -- Structure describing a section of memory.  */
struct iovec
{
  /* Starting address.  */
  void *iov_base;
  /* Length in bytes.  */
  size_t iov_len;
};


/* Read data from file descriptor FD, and put the result in the
   buffers described by IOVEC, which is a vector of COUNT 'struct iovec's.
   The buffers are filled in the order specified.
   Operates just like 'read' (see <unistd.h>) except that data are
   put in IOVEC instead of a contiguous buffer. */
extern ssize_t readv( int fd, const struct iovec *vector, int count );

/* Write data pointed by the buffers described by IOVEC, which
   is a vector of COUNT 'struct iovec's, to file descriptor FD.
   The data is written in the order specified.
   Operates just like 'write' (see <unistd.h>) except that the data
   are taken from IOVEC instead of a contiguous buffer. */
extern ssize_t writev( int fd, const struct iovec *iovec, int count );

/* Read data from file descriptor FD at the given position OFFSET
   without change the file pointer, and put the result in the buffers
   described by IOVEC, which is a vector of COUNT 'struct iovec's.
   The buffers are filled in the order specified.  Operates just like
   'pread' (see <unistd.h>) except that data are put in IOVEC instead
   of a contiguous buffer.*/
extern ssize_t preadv( int fd, const struct iovec *iovec, int count,
                       off_t offset );

/* Write data pointed by the buffers described by IOVEC, which is a
   vector of COUNT 'struct iovec's, to file descriptor FD at the given
   position OFFSET without change the file pointer.  The data is
   written in the order specified.  Operates just like 'pwrite' (see
   <unistd.h>) except that the data are taken from IOVEC instead of a
   contiguous buffer. */
extern ssize_t pwritev( int fd, const struct iovec *iovec, int count,
                        off_t offset);

#endif /* sys/uio.h */
