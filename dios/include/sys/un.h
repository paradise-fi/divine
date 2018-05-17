#ifndef _SYS_UN_H
#define _SYS_UN_H  1

#include <sys/types.h>

#define UNIX_PATH_MAX  107

/* Structure describing the address of an AF_LOCAL (aka AF_UNIX) socket.  */
struct sockaddr_un  {
    __sa_family_t sun_family;
    char sun_path[UNIX_PATH_MAX];    /* Path name.  */
};

#endif
