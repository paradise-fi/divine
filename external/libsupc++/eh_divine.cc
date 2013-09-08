#include <divine.h>
#include "unwind-cxx.h"
#include <limits.h>

using namespace __cxxabiv1;

struct LPReturn
{
    _Unwind_Exception *e;
    int handle;
};

static inline LPReturn lpreturn( _Unwind_Exception *e, int h )
{
    LPReturn rv;
    rv.e = e;
    rv.handle = h;
    return rv;
}

extern "C" {

/* The return value of the personality function becomes the return value of the
 * landingpad instruction itself. Care must be taken for the type to match what
 * the C++ frontend has generated for the landingpad. */
LPReturn __gxx_personality_v0( __cxa_exception *e )
{
    return lpreturn( &e->unwindHeader, e->handlerSwitchValue );
}

void __cxa_call_unexpected( void *) {
    __divine_assert( 0 ); // TODO
}

void __cxa_throw_divine( __cxa_exception *e )
{
    int frameid = -1, destination = 0;
    int handler = -1;
    _DivineLP_Info *lp = 0;
    typedef LPReturn (*Personality)( __cxa_exception * );
    Personality personality = 0;

    /* TODO: Check nativeness of e */

    while ( handler < 0 ) {
        lp = __divine_landingpad( frameid );

        if ( !lp ) {
            __divine_assert( 0 ); /* FIXME call terminate or something */
            __divine_unwind( INT_MIN );
        }

        if ( (lp->cleanup || lp->clause_count) && !destination )
            destination = frameid;

        int cc = lp->clause_count;
        for ( int i = 0; i < cc; i ++ ) {
            int filter = lp->clause[i].is_filter;
            void *tag = lp->clause[i].tag;
            if ( !filter && ( !tag || tag == e->exceptionType ) )
            {
                handler = i;
                personality = (Personality) lp->personality;
                break; // found the right lp, stop looking
            }
        }

        -- frameid;
    }
    e->handlerSwitchValue = handler;
    __divine_unwind( destination, personality ? personality( e ) : lpreturn( 0, 0 ) );
}

}

#include "libsupc++/unwind.h"

_Unwind_Reason_Code _Unwind_RaiseException (struct _Unwind_Exception *) { __divine_assert( 0 ); return _URC_NO_REASON; }
_Unwind_Reason_Code _Unwind_Resume_or_Rethrow (struct _Unwind_Exception *) { __divine_assert( 0 ); return _URC_NO_REASON; }
_Unwind_Reason_Code _Unwind_ForcedUnwind (struct _Unwind_Exception *, _Unwind_Stop_Fn, void *) { __divine_assert( 0 ); return _URC_NO_REASON; }
void _Unwind_DeleteException (struct _Unwind_Exception *) { __divine_assert( 0 ); }
_Unwind_Word _Unwind_GetGR (struct _Unwind_Context *, int) { __divine_assert( 0 ); return 0; }
void _Unwind_SetGR (struct _Unwind_Context *, int, _Unwind_Word) { __divine_assert( 0 ); }
_Unwind_Ptr _Unwind_GetIP (struct _Unwind_Context *) { __divine_assert( 0 ); return 0; }
_Unwind_Ptr _Unwind_GetIPInfo (struct _Unwind_Context *, int *) { __divine_assert( 0 ); return 0; }
void _Unwind_SetIP (struct _Unwind_Context *, _Unwind_Ptr) { __divine_assert( 0 ); }
_Unwind_Word _Unwind_GetCFA (struct _Unwind_Context *) { __divine_assert( 0 ); return 0; }
void *_Unwind_GetLanguageSpecificData (struct _Unwind_Context *) { __divine_assert( 0 ); return 0; }
_Unwind_Ptr _Unwind_GetRegionStart (struct _Unwind_Context *) { __divine_assert( 0 ); return 0; }
void _Unwind_SjLj_Register (struct SjLj_Function_Context *) { __divine_assert( 0 ); }
void _Unwind_SjLj_Unregister (struct SjLj_Function_Context *) { __divine_assert( 0 ); }
_Unwind_Word _Unwind_GetBSP (struct _Unwind_Context *) { __divine_assert( 0 ); return 0; }
_Unwind_Ptr _Unwind_GetDataRelBase (struct _Unwind_Context *) { __divine_assert( 0 ); return 0; }
_Unwind_Ptr _Unwind_GetTextRelBase (struct _Unwind_Context *) { __divine_assert( 0 ); return 0; }
void * _Unwind_FindEnclosingFunction (void *pc) { __divine_assert( 0 ); return 0; }
