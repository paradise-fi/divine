/*-
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)stat.h	8.9 (Berkeley) 8/17/94
 */

#ifndef _SYS_STAT_H_
#define	_SYS_STAT_H_

#include <time.h>
#include <sys/cdefs.h>
#include <sys/types.h>

#define _HOST_stat stat
#include <sys/hostabi.h>
#undef _HOST_stat

#if __POSIX_VISIBLE >= 200809 || __BSD_VISIBLE
#define	st_atime		st_atim.tv_sec
#define	st_mtime		st_mtim.tv_sec
#define	st_ctime		st_ctim.tv_sec
#define	__st_birthtime		__st_birthtim.tv_sec
#endif
#if __BSD_VISIBLE
#define	st_atimespec		st_atim
#define	st_atimensec		st_atim.tv_nsec
#define	st_mtimespec		st_mtim
#define	st_mtimensec		st_mtim.tv_nsec
#define	st_ctimespec		st_ctim
#define	st_ctimensec		st_ctim.tv_nsec
#define	__st_birthtimespec	__st_birthtim
#define	__st_birthtimensec	__st_birthtim.tv_nsec
#endif

#define	S_ISUID _HOST_S_ISUID			/* set user id on execution */
#define	S_ISGID _HOST_S_ISGID			/* set group id on execution */
#if __BSD_VISIBLE
#define	S_ISTXT	S_ISVTX		        /* sticky bit */
#endif

#define	S_IRWXU _HOST_S_IRWXU			/* RWX mask for owner */
#define	S_IRUSR _HOST_S_IRUSR			/* R for owner */
#define	S_IWUSR _HOST_S_IWUSR			/* W for owner */
#define	S_IXUSR _HOST_S_IXUSR			/* X for owner */

#if __BSD_VISIBLE
#define	S_IREAD		S_IRUSR
#define	S_IWRITE	S_IWUSR
#define	S_IEXEC		S_IXUSR
#endif

#define	S_IRWXG _HOST_S_IRWXG			/* RWX mask for group */
#define	S_IRGRP _HOST_S_IRGRP			/* R for group */
#define	S_IWGRP _HOST_S_IWGRP			/* W for group */
#define	S_IXGRP _HOST_S_IXGRP			/* X for group */

#define	S_IRWXO _HOST_S_IRWXO			/* RWX mask for other */
#define	S_IROTH _HOST_S_IROTH			/* R for other */
#define	S_IWOTH _HOST_S_IWOTH			/* W for other */
#define	S_IXOTH _HOST_S_IXOTH			/* X for other */

#if __XPG_VISIBLE || __BSD_VISIBLE
#define	S_IFMT   _HOST_S_IFMT		/* type of file mask */
#define	S_IFIFO  _HOST_S_IFIFO		/* named pipe (fifo) */
#define	S_IFCHR  _HOST_S_IFCHR		/* character special */
#define	S_IFDIR  _HOST_S_IFDIR		/* directory */
#define	S_IFBLK  _HOST_S_IFBLK		/* block special */
#define	S_IFREG  _HOST_S_IFREG		/* regular */
#define	S_IFLNK  _HOST_S_IFLNK		/* symbolic link */
#define	S_IFSOCK _HOST_S_IFSOCK		/* socket */
#define	S_ISVTX  _HOST_S_ISVTX		/* save swapped text even after use */
#endif

#define	S_ISDIR(m)	((m & 0170000) == 0040000)	/* directory */
#define	S_ISCHR(m)	((m & 0170000) == 0020000)	/* char special */
#define	S_ISBLK(m)	((m & 0170000) == 0060000)	/* block special */
#define	S_ISREG(m)	((m & 0170000) == 0100000)	/* regular file */
#define	S_ISFIFO(m)	((m & 0170000) == 0010000)	/* fifo */
#if __POSIX_VISIBLE >= 200112 || __BSD_VISIBLE
#define	S_ISLNK(m)	((m & 0170000) == 0120000)	/* symbolic link */
#define	S_ISSOCK(m)	((m & 0170000) == 0140000)	/* socket */
#endif

#if __POSIX_VISIBLE >= 200809
/* manadated to be present, but permitted to always return zero */
#define	S_TYPEISMQ(m)	0
#define	S_TYPEISSEM(m)	0
#define	S_TYPEISSHM(m)	0
#endif

#if __BSD_VISIBLE
#define	ACCESSPERMS	(S_IRWXU|S_IRWXG|S_IRWXO)	/* 00777 */
							/* 07777 */
#define	ALLPERMS	(S_ISUID|S_ISGID|S_ISTXT|S_IRWXU|S_IRWXG|S_IRWXO)
							/* 00666 */
#define	DEFFILEMODE	(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)

#define	S_BLKSIZE	512		/* block size used in the stat struct */

/*
 * Definitions of flags stored in file flags word.
 *
 * Super-user and owner changeable flags.
 */
#define	UF_SETTABLE	0x0000ffff	/* mask of owner changeable flags */
#define	UF_NODUMP	0x00000001	/* do not dump file */
#define	UF_IMMUTABLE	0x00000002	/* file may not be changed */
#define	UF_APPEND	0x00000004	/* writes to file may only append */
#define	UF_OPAQUE	0x00000008	/* directory is opaque wrt. union */
/*
 * Super-user changeable flags.
 */
#define	SF_SETTABLE	0xffff0000	/* mask of superuser changeable flags */
#define	SF_ARCHIVED	0x00010000	/* file is archived */
#define	SF_IMMUTABLE	0x00020000	/* file may not be changed */
#define	SF_APPEND	0x00040000	/* writes to file may only append */

#ifdef _KERNEL
/*
 * Shorthand abbreviations of above.
 */
#define	OPAQUE		(UF_OPAQUE)
#define	APPEND		(UF_APPEND | SF_APPEND)
#define	IMMUTABLE	(UF_IMMUTABLE | SF_IMMUTABLE)
#endif /* _KERNEL */
#endif /* __BSD_VISIBLE */

#if __POSIX_VISIBLE >= 200809
#define	UTIME_NOW	-2L
#define	UTIME_OMIT	-1L
#endif /* __POSIX_VISIBLE */

__BEGIN_DECLS
int	chmod(const char *, mode_t) __nothrow;
int	fstat(int, struct stat *) __nothrow;
int	mknod(const char *, mode_t, dev_t) __nothrow;
int	mkdir(const char *, mode_t) __nothrow;
int	mkfifo(const char *, mode_t) __nothrow;
int	stat(const char *, struct stat *) __nothrow;
mode_t	umask(mode_t) __nothrow;
#if __POSIX_VISIBLE >= 200112L || __XPG_VISIBLE >= 420 || __BSD_VISIBLE
int	fchmod(int, mode_t) __nothrow;
int	lstat(const char *, struct stat *) __nothrow;
#endif
#if __POSIX_VISIBLE >= 200809
int	fchmodat(int, const char *, mode_t, int) __nothrow;
int	fstatat(int, const char *, struct stat *, int) __nothrow;
int	mkdirat(int, const char *, mode_t) __nothrow;
int	mkfifoat(int, const char *, mode_t) __nothrow;
int	mknodat(int, const char *, mode_t, dev_t) __nothrow;
int	utimensat(int, const char *, const struct timespec [2], int) __nothrow;
int	futimens(int, const struct timespec [2]) __nothrow;
#endif
#if __BSD_VISIBLE
int	chflags(const char *, unsigned int) __nothrow;
int	chflagsat(int, const char *, unsigned int, int) __nothrow;
int	fchflags(int, unsigned int) __nothrow;
int	isfdtype(int, int) __nothrow;
#endif
__END_DECLS
#endif /* !_SYS_STAT_H_ */
