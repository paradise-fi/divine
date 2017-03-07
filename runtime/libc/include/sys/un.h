#ifndef _SYS_UN_H
#define _SYS_UN_H  1

#include <bits/sockaddr.h>

#define UNIX_PATH_MAX  107

/* Structure describing the address of an AF_LOCAL (aka AF_UNIX) socket.  */
struct sockaddr_un  {
    __SOCKADDR_COMMON (sun_);
    char sun_path[UNIX_PATH_MAX];    /* Path name.  */
};

#endif
