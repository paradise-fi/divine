/* TAGS: c++ */
/* VERIFY_OPTS: */
/* CC_OPTS: -I$SRC_ROOT/bricks/ -std=c++17 */
/* PROGRAM_OPTS: empty */

#define BRICK_UNITTEST_REG
#define BRICK_UNITTEST_MAIN
#include <brick-unittest>

[[ gnu::constructor( 0 ) ]] static void _test_set_terminate_handler()
{
    std::set_terminate( [] { exit( 0 ); __builtin_unreachable(); } );
}

// V: no_oom      V_OPT: -o nofail:malloc
// V: sim_oom     V_OPT:
