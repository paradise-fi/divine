/* USE_GTEST: 1 */
/* VERIFY_OPTS: -o nofail:malloc */

#include <gtest/gtest.h>

TEST( Buggy, Foo )
{
    assert( 0 ); /* ERROR */
}
