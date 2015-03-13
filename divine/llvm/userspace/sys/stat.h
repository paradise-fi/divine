/* Copyright (C) 1991,1992,1995-2004,2005,2006 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

/*
 *	POSIX Standard: 5.6 File Characteristics	<sys/stat.h>
 */

#ifndef	_SYS_STAT_H
#define	_SYS_STAT_H	1

#include <sys/types.h>
#include <bits/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

#define S_IFMT   __S_IFMT
#define S_IFDIR  __S_IFDIR
#define S_IFCHR  __S_IFCHR
#define S_IFBLK  __S_IFBLK
#define S_IFREG  __S_IFREG
#define S_IFIFO  __S_IFIFO
#define S_IFLNK  __S_IFLNK
#define S_IFSOCK __S_IFSOCK

/* Test macros for file types.	*/

#define   __S_ISTYPE(mode, mask)	(((mode) & __S_IFMT) == (mask))

#define   S_ISDIR(mode)  __S_ISTYPE((mode), __S_IFDIR)
#define   S_ISCHR(mode)  __S_ISTYPE((mode), __S_IFCHR)
#define   S_ISBLK(mode)  __S_ISTYPE((mode), __S_IFBLK)
#define   S_ISREG(mode)  __S_ISTYPE((mode), __S_IFREG)
#define   S_ISFIFO(mode) __S_ISTYPE((mode), __S_IFIFO)
#define   S_ISLNK(mode)  __S_ISTYPE((mode), __S_IFLNK)
#define   S_ISSOCK(mode) __S_ISTYPE((mode), __S_IFSOCK)

/* These are from POSIX.1b.  If the objects are not implemented using separate
   distinct file types, the macros always will evaluate to zero.  Unlike the
   other S_* macros the following three take a pointer to a `struct stat'
   object as the argument.  */
#define S_TYPEISMQ(buf) __S_TYPEISMQ(buf)
#define S_TYPEISSEM(buf) __S_TYPEISSEM(buf)
#define S_TYPEISSHM(buf) __S_TYPEISSHM(buf)


/* Protection bits.  */

#define	S_ISUID    __S_ISUID	/* Set user ID on execution.  */
#define	S_ISGID    __S_ISGID	/* Set group ID on execution.  */

/* Save swapped text after use (sticky bit).  This is pretty well obsolete.  */
#define S_ISVTX	__S_ISVTX

#define	S_IRUSR	__S_IREAD	/* Read by owner.  */
#define	S_IWUSR	__S_IWRITE	/* Write by owner.  */
#define	S_IXUSR	__S_IEXEC	/* Execute by owner.  */
/* Read, write, and execute by owner.  */
#define	S_IRWXU	(__S_IREAD|__S_IWRITE|__S_IEXEC)

#define S_IREAD	S_IRUSR
#define S_IWRITE	S_IWUSR
#define S_IEXEC	S_IXUSR

#define	S_IRGRP	(S_IRUSR >> 3)	/* Read by group.  */
#define	S_IWGRP	(S_IWUSR >> 3)	/* Write by group.  */
#define	S_IXGRP	(S_IXUSR >> 3)	/* Execute by group.  */
/* Read, write, and execute by group.  */
#define	S_IRWXG	(S_IRWXU >> 3)

#define	S_IROTH	(S_IRGRP >> 3)	/* Read by others.  */
#define	S_IWOTH	(S_IWGRP >> 3)	/* Write by others.  */
#define	S_IXOTH	(S_IXGRP >> 3)	/* Execute by others.  */
/* Read, write, and execute by others.  */
#define	S_IRWXO	(S_IRWXG >> 3)


/* Macros for common mode bit masks.  */
#define ACCESSPERMS (S_IRWXU|S_IRWXG|S_IRWXO) /* 0777 */
#define ALLPERMS (S_ISUID|S_ISGID|S_ISVTX|S_IRWXU|S_IRWXG|S_IRWXO)/* 07777 */
#define DEFFILEMODE (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)/* 0666*/

#define S_BLKSIZE	512	/* Block size for `st_blocks'.  */

#define FS_NOINLINE __attribute__((noinline))

FS_NOINLINE int fstat( int fd, struct stat *buf );
FS_NOINLINE int stat( const char *path, struct stat *buf );
FS_NOINLINE int lstat( const char *path, struct stat *buf );
FS_NOINLINE int fstatat( int dirfd, const char *path, struct stat *buf, int flags );

FS_NOINLINE int chmod( const char *path, mode_t mode );
FS_NOINLINE int fchmod( int fd, mode_t mode );
FS_NOINLINE int fchmodat( int dirfd, const char *path, mode_t mode, int flags );

FS_NOINLINE mode_t umask( mode_t mask );

FS_NOINLINE int mkdir( const char *path, mode_t mode );
FS_NOINLINE int mkdirat( int dirfd, const char *path, mode_t mode );

FS_NOINLINE int mkfifo( const char *path, mode_t mode );
FS_NOINLINE int mkfifoat( int dirfd, const char *path, mode_t mode );

#undef FS_NOINLINE

#ifdef __cplusplus
}// extern "C"
#endif

#endif

