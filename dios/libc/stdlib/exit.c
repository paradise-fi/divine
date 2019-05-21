#include <stdlib.h>

void exit( int status )
{
    /* atexit is sorted out by __cxa_finalize */
    _Exit( status );
}
