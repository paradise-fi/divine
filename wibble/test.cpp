#include <wibble/test.h>

namespace wibble {
int assertFailure = 0;

void assert_die_fn( Location l )
{
    AssertFailed f( l );
}

}
