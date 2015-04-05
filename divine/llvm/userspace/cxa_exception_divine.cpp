#include <divine.h>
#include "unwind.h"
#include <src/cxa_exception.hpp>
#include <src/private_typeinfo.h>
#include <limits.h>

using namespace __cxxabiv1;

struct LPReturn
{
    void *e;
    int h;
} __attribute__((packed));

extern "C" {

/* The return value of the personality function becomes the return value of the
 * landingpad instruction itself. Care must be taken for the type to match what
 * the C++ frontend has generated for the landingpad. */
LPReturn __gxx_personality_v0( __cxa_exception *e )
{
    LPReturn rv;
    rv.e = &e->unwindHeader;
    rv.h = e->handlerSwitchValue;
    return rv;
}

void __cxa_call_unexpected( void *) {
    __divine_problem( 1, "unexpected exception" );
}

void __cxa_throw_divine( __cxa_exception *e )
{
    int frameid = -1, destination = 0;
    int handler = -1;
    _DivineLP_Info *lp = 0;
    typedef LPReturn (*Personality)( __cxa_exception * );
    Personality personality = 0;
    e->adjustedPtr = &e->unwindHeader + 1;
    auto eType = static_cast< const __shim_type_info * >( e->exceptionType );

    /* TODO: Check nativeness of e */

    while ( handler < 0 ) {
        lp = __divine_landingpad( frameid );

        if ( !lp ) {
            __divine_problem( 1, "unhandled exception" );
            __divine_unwind( INT_MIN );
        }

        if ( (lp->cleanup || lp->clause_count) && !destination )
            destination = frameid;

        int cc = lp->clause_count;
        for ( int i = 0; i < cc; i ++ ) {
            int type_id = lp->clause[i].type_id;
            auto tag = static_cast< const __shim_type_info * >( lp->clause[i].tag );
            if ( type_id > 0 && (
                     !tag || tag == eType || tag->can_catch( eType, e->adjustedPtr ) ) )
            {
                handler = type_id;
                personality = (Personality) lp->personality;
                break; // found the right lp, stop looking
            }
            /* TODO: Handle filters aka exception specifications, aka
               int blee(...) throws( stuff ). The spec wants us to call the
               unexpected handler. */
        }

        __divine_free( lp );
        -- frameid;
    }

    e->handlerSwitchValue = handler;
    LPReturn ret = { 0, 0 };
    if ( personality )
        ret = personality( e );

    __divine_unwind( destination, ret.e, ret.h );
}

}

_Unwind_Reason_Code _Unwind_RaiseException (struct _Unwind_Exception *) { __divine_assert( 0 ); return _URC_NO_REASON; }
_Unwind_Reason_Code _Unwind_Resume_or_Rethrow (struct _Unwind_Exception *) { __divine_assert( 0 ); return _URC_NO_REASON; }
_Unwind_Reason_Code _Unwind_ForcedUnwind (struct _Unwind_Exception *, _Unwind_Stop_Fn, void *) { __divine_assert( 0 ); return _URC_NO_REASON; }
void _Unwind_DeleteException (struct _Unwind_Exception *) {} /* do nothing */
void _Unwind_SjLj_Register (struct SjLj_Function_Context *) { __divine_assert( 0 ); }
void _Unwind_SjLj_Unregister (struct SjLj_Function_Context *) { __divine_assert( 0 ); }
void * _Unwind_FindEnclosingFunction (void *pc) { __divine_assert( 0 ); return 0; }
