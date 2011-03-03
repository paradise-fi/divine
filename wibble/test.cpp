#include <wibble/test.h>

namespace wibble {
int assertFailure = 0;

void assert_die_fn( Location l )
{
    AssertFailed f( l );
#ifdef NDEBUG // in the NDEBUG case, the above is a no-op
    abort();
#endif
}

}
