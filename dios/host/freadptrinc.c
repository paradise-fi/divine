/* Skipping input from a FILE stream.
   Copyright (C) 2007-2018 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include "stdio-impl.h"

/*
 * NOTE: This comes from freadseek.c in gnulib, where it is used as a local helper function.
 */

#if !HAVE___FREADPTRINC

/* Increment the in-memory pointer.  INCREMENT must be at most the buffer size
   returned by freadptr().
   This is very cheap (no system calls).  */
void
__freadptrinc (FILE *fp, size_t increment)
{
  /* Keep this code in sync with freadptr!  */
#if HAVE___FREADPTRINC              /* musl libc */
  __freadptrinc (fp, increment);
#elif defined _IO_EOF_SEEN || defined _IO_ftrylockfile || __GNU_LIBRARY__ == 1
  /* GNU libc, BeOS, Haiku, Linux libc5 */
  fp->_IO_read_ptr += increment;
#elif defined __sferror || defined __DragonFly__ || defined __ANDROID__
  /* FreeBSD, NetBSD, OpenBSD, DragonFly, Mac OS X, Cygwin, Minix 3, Android */
  fp_->_p += increment;
  fp_->_r -= increment;
#elif defined __EMX__               /* emx+gcc */
  fp->_ptr += increment;
  fp->_rcount -= increment;
#elif defined __minix               /* Minix */
  fp_->_ptr += increment;
  fp_->_count -= increment;
#elif defined _IOERR                /* AIX, HP-UX, IRIX, OSF/1, Solaris, OpenServer, mingw, MSVC, NonStop Kernel, OpenVMS */
  fp_->_ptr += increment;
  fp_->_cnt -= increment;
#elif defined __UCLIBC__            /* uClibc */
# ifdef __STDIO_BUFFERS
  fp->__bufpos += increment;
# else
  abort ();
# endif
#elif defined __QNX__               /* QNX */
  fp->_Next += increment;
#elif defined __MINT__              /* Atari FreeMiNT */
  fp->__bufp += increment;
#elif defined EPLAN9                /* Plan9 */
  fp->rp += increment;
#elif defined SLOW_BUT_NO_HACKS     /* users can define this */
#else
 #error "Unsupported platform. This code comes from gnulib freadseek.c, perhaps a new version of that file is available with support for this system."
#endif
}

#endif
