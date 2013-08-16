#ifndef _PDCLIB_UNISTD_H
#define _PDCLIB_UNISTD_H

#include <sys/types.h>
#include <errno.h>
#include <stddef.h>

_PDCLIB_BEGIN_EXTERN_C

// IO operations ----------------------------------------------------

/* Close the file descriptor FD. */
extern int close ( int fd );

/* Read NBYTES into BUF from FD.  Return the
   number read, -1 for errors or 0 for EOF. */
extern ssize_t read ( int fd, void *buf, size_t nbytes );

/* Write N bytes of BUF to FD.  Return the number written, or -1. */
extern ssize_t write ( int fd, const void *buf, size_t n );

/* Read NBYTES into BUF from FD at the given position OFFSET without
   changing the file pointer.  Return the number read, -1 for errors
   or 0 for EOF. */
extern ssize_t pread ( int fd, void *buf, size_t nbytes,
                       off_t offset );

/* Write N bytes of BUF to FD at the given position OFFSET without
   changing the file pointer.  Return the number written, or -1. */
extern ssize_t pwrite ( int fd, const void *buf, size_t n,
                        off_t offset );

/* Move FD's file position to OFFSET bytes from the beginning of the file
   (if WHENCE is SEEK_SET), the current position (if WHENCE is SEEK_CUR),
   or the end of the file (if WHENCE is SEEK_END).
   Return the new file position.  */
off_t lseek( int fildes, off_t offset, int whence );

/* The link function makes a new link to the existing file named by oldname,
   under the new name newname.
 */
int link ( const char *oldname, const char *newname );

/* Remove the link NAME.  */
extern int unlink ( const char *name );

// System parameters ------------------------------------------------

/* Get the `_SC_*' symbols for the NAME argument to `sysconf'. */
#include <bits/confname.h>

/* Get the value of the system variable NAME.  */
extern long int sysconf ( int name );

/* Return the number of bytes in a page.  This is the system's page size,
   which is not necessarily the same as the hardware page size.  */
extern int getpagesize ( void );

_PDCLIB_END_EXTERN_C

/* Evaluate EXPRESSION, and repeat as long as it returns -1 with `errno'
   set to EINTR.  */

# define TEMP_FAILURE_RETRY(expression) \
  (__extension__							      \
    ({ long int __result;						      \
       do __result = (long int) (expression);				      \
       while (__result == -1L && errno == EINTR);			      \
       __result; }))

#endif /* unistd.h  */
