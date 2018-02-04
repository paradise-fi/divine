#include <sys/resource.h>
#include <errno.h>

int getrlimit(int resource, struct rlimit *rlp)
{
    rlim_t limit;
    switch( resource )
    {
        case RLIMIT_CORE: limit = 0; break; // no core file
        case RLIMIT_CPU:
        case RLIMIT_FSIZE:
        case RLIMIT_DATA:
        case RLIMIT_STACK:
        case RLIMIT_RSS: limit = RLIM_INFINITY; break;
        default: errno = EINVAL; return -1;
    }
    rlp->rlim_cur = rlp->rlim_max = limit;
    return 0;
}

int setrlimit(int resource, const struct rlimit *rlp)
{
    switch( resource )
    {
        case RLIMIT_CORE:
        case RLIMIT_CPU:
        case RLIMIT_FSIZE:
        case RLIMIT_DATA:
        case RLIMIT_STACK:
        case RLIMIT_RSS: break;
        default: errno = EINVAL; return -1;
    }
    if ( rlp->rlim_cur > rlp->rlim_max )
    {
        errno = EINVAL;
        return -1;
    }
    return 0;
}
