#ifndef _MNTENT_H
#define _MNTENT_H

#include <stdio.h>

__BEGIN_DECLS

/* Structure describing a mount table entry.  */
struct mntent
{
    char *mnt_fsname;		/* Device or server for filesystem.  */
    char *mnt_dir;		/* Directory mounted on.  */
    char *mnt_type;		/* Type of filesystem: ufs, nfs, etc.  */
    char *mnt_opts;		/* Comma-separated options for fs.  */
    int mnt_freq;		/* Dump frequency (in days).  */
    int mnt_passno;		/* Pass number for `fsck'.  */
};

#define MOUNTED "/etc/mtab"

FILE *setmntent (const char *__file, const char *__mode) __nothrow;
struct mntent *getmntent (FILE *__stream) __nothrow;
int addmntent (FILE *__restrict __stream, const struct mntent *__restrict __mnt) __nothrow;
int endmntent (FILE *__stream) __nothrow;
char *hasmntopt (const struct mntent *__mnt, const char *__opt) __nothrow;

__END_DECLS

#endif
