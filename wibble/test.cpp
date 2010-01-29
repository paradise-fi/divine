#include <wibble/test.h>

int assertFailure = 0;

void assert_die_fn( Location l )
{
    AssertFailed f( l );
#ifdef NDEBUG // in the NDEBUG case, the above is a no-op
    abort();
#endif
}

