#ifndef _FS_STAT_H_
#define _FS_STAT_H_

#ifdef __cplusplus
extern "C" {
#endif

struct stat {
    int st_dev;// 0
    int st_ino;
    int st_mode;
    int st_nlink;
    int st_uid;
    int st_gid;
    int st_rdev;// 0
    int st_size;
    int st_blksize;
    int st_blocks;
    int st_atime;// 0
    int st_mtime;// 0
    int st_ctime;// 0
};

#ifdef __cplusplus
}
#endif


#endif
