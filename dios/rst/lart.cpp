#include <rst/lart.h>
#include <sys/task.h>
#include <rst/common.hpp>

#include <string.h>

using namespace __dios::rst::abstract;

/* Abstract versions of arguments and return values are passed in a 'stash', a
 * global (per-thread) location. On 'unstash', we clear the stash for two
 * reasons: 1) it flags up otherwise hard-to-pin bugs where mismatched
 * stash/unstash reads an old value of the stash, and 2) when verification
 * encounters a possible diamond (the explicit content of the state is the
 * same, e.g.  after an if/else) and we leave old stashes around, it might so
 * happen that the stashed values in the candidate states are of different
 * types, leading to a type mismatch in the comparison.
 *
 * TODO The second problem might have to be taclked in the verification
 * algorithm anyway, since there might be other ways to trigger the same
 * problem. */

extern "C"
{
    _LART_INTERFACE void * __lart_unstash()
    {
        auto &stash = __dios_this_task()->__rst_stash;
        auto rv = stash;
        stash = nullptr;
        return rv;
    }

    _LART_INTERFACE void __lart_stash( void * val )
    {
        __dios_this_task()->__rst_stash = val;
    }

    _LART_INTERFACE void __lart_freeze( void * value, void * addr )
    {
        poke_object( value, addr );
    }

    _LART_INTERFACE void * __lart_thaw( void * addr )
    {
        return peek_object< void * >( addr );
    }
}
