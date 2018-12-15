#ifndef _SYS_MOUNT_H_
#define _SYS_MOUNT_H_

#include <sys/cdefs.h>

/*
 * file system statistics
 */

#define	MFSNAMELEN	16	/* length of fs type name, including nul */
#define	MNAMELEN	90	/* length of buffer for returned name */

typedef struct { int32_t val[2]; } fsid_t;
union mount_info { char __align[160]; };

/* new statfs structure with mount options and statvfs fields */
struct statfs {
	uint32_t	f_flags;	/* copy of mount flags */
	uint32_t	f_bsize;	/* file system block size */
	uint32_t	f_iosize;	/* optimal transfer block size */

					/* unit is f_bsize */
	uint64_t  	f_blocks;	/* total data blocks in file system */
	uint64_t  	f_bfree;	/* free blocks in fs */
	int64_t  	f_bavail;	/* free blocks avail to non-superuser */

	uint64_t 	f_files;	/* total file nodes in file system */
	uint64_t  	f_ffree;	/* free file nodes in fs */
	int64_t  	f_favail;	/* free file nodes avail to non-root */

	uint64_t  	f_syncwrites;	/* count of sync writes since mount */
	uint64_t  	f_syncreads;	/* count of sync reads since mount */
	uint64_t  	f_asyncwrites;	/* count of async writes since mount */
	uint64_t  	f_asyncreads;	/* count of async reads since mount */

	fsid_t	   	f_fsid;		/* file system id */
	uint32_t	f_namemax;      /* maximum filename length */
	uid_t	   	f_owner;	/* user that mounted the file system */
	uint64_t  	f_ctime;	/* last mount [-u] time */

	char f_fstypename[MFSNAMELEN];	/* fs type name */
	char f_mntonname[MNAMELEN];	/* directory on which mounted */
	char f_mntfromname[MNAMELEN];	/* mounted file system */
	char f_mntfromspec[MNAMELEN];	/* special for mount request */
	union mount_info mount_info;	/* per-filesystem mount options */
};

__BEGIN_DECLS
int	fstatfs(int, struct statfs *) __nothrow;
// int	getfh(const char *, fhandle_t *) __nothrow;
int	getfsstat(struct statfs *, size_t, int) __nothrow;
int	getmntinfo(struct statfs **, int) __nothrow;
int	mount(const char *, const char *, int, void *) __nothrow;
int	statfs(const char *, struct statfs *) __nothrow;
int	unmount(const char *, int) __nothrow;
#if 0 && __BSD_VISIBLE
struct stat;
int	fhopen(const fhandle_t *, int);
int	fhstat(const fhandle_t *, struct stat *);
int	fhstatfs(const fhandle_t *, struct statfs *);
#endif /* __BSD_VISIBLE */
__END_DECLS

#endif /* !_SYS_MOUNT_H_ */
