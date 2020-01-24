/* TAGS: star min */
/* VERIFY_OPTS: --lamp unit -o nofail:malloc */

#include <sys/lamp.h>
#include <cassert>

char * store_ten( char * aggr )
{
    auto idx = __lamp_any_i64();
    aggr[idx] = 10;
    return aggr;
}

int main()
{
    auto aggr = __lamp_any_array();
    auto res = store_ten( aggr );
}
