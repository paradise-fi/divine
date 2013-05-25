/* Includes */
#include <divine.h>
#include <cstdlib>
#include <stdlib.h>
#include <stdio.h>

/* Memory allocation */
void * malloc( size_t size ) throw() {
    __divine_interrupt_mask();
#ifdef TRACE
    trace( "malloc( %d ) called..", size );
#endif
    __divine_interrupt_mask();
#ifdef NO_MALLOC_FAILURE
    return __divine_malloc( size ); // always success
#else
    if ( __divine_choice( 2 ) ) {
        return __divine_malloc( size ); // success
    } else {
        return NULL; // failure
    }
#endif
}

/* TODO malloc currently gives zeroed memory */
void *calloc( size_t n, size_t size ) throw() { return malloc( n * size ); }
void *realloc( void *ptr, size_t size ) throw() { __divine_assert( 0 ); return 0; }

void free( void * p) throw() {
    __divine_interrupt_mask();
#ifdef TRACE
    trace( "free( %p ) called..", p );
#endif
    __divine_free( p );
}

/* IOStream */

void _ZNSt8ios_base4InitC1Ev( void ) { // std::ios_base::Init
    // TODO?
}

void *__dso_handle; /* this is emitted by clang for calls to __cxa_exit for whatever reason */

extern "C" int __cxa_atexit( void ( *func ) ( void * ), void *arg, void *dso_handle ) {
    // TODO
    ( void ) func;
    ( void ) arg;
    ( void ) dso_handle;
    return 0;
}

extern "C" void abort_message( const char * ) { __divine_assert( 0 ); }
extern "C" void *dlsym( void *, void * ) { __divine_assert( 0 ); return 0; } // oh golly...

extern "C" void *__errno_location() { __divine_assert( 0 ); return 0; }

extern "C" { /* POSIX kernel APIs */

    void raise( int ) { __divine_assert( 0 ); }
    int unlink( const char * ) { __divine_assert( 0 ); return 0; }
    size_t read(int, void *, size_t) { __divine_assert( 0 ); return 0; }
    size_t write(int, const void *, size_t) { __divine_assert( 0 ); return 0; }
    int lseek(int, int, int) { __divine_assert( 0 ); return 0; }
    int close(int) { __divine_assert( 0 ); return 0; }

}

/* currently missing from our pdclib */
extern "C" FILE *tmpfile() throw() { __divine_assert( 0 ); return 0; }


extern "C" { /* pdclib glue functions */
    int _PDCLIB_rename( const char *, const char * ) { __divine_assert( 0 ); return 0; }
    int _PDCLIB_open( const char *, int ) { __divine_assert( 0 ); return 0; }
    void _PDCLIB_Exit( int ) { __divine_assert( 0 ); }
}

/******** unwind library stubs */

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
