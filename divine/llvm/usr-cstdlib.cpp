/* Includes */
#include "cstdlib"

/* Memory allocation */
void * malloc( size_t size ) {
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

/* TODO malloc currently gives zeroed memory too */
void *calloc( size_t size ) { return malloc( size ); }
void *realloc( void *ptr, size_t size ) { __divine_assert( 0 ); return 0; }

void free( void * p) {
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

/* Exit */
void exit( int exit_code ) {
    // TODO
    ( void ) exit_code;
}

int __cxa_atexit( void ( *func ) ( void * ), void *arg, void *dso_handle ) {
    // TODO
    ( void ) func;
    ( void ) arg;
    ( void ) dso_handle;
    return 0;
}

int atexit( void ( *func )( void ) ) {
    // TODO
    ( void ) func;
    return 0;
}

void *stderr = 0;

extern "C" void fputs() { __divine_assert( 0 ); }
extern "C" void abort() { __divine_assert( 0 ); }
extern "C" void abort_message( const char * ) { __divine_assert( 0 ); }

extern "C" void *dlsym(void *, void *) { __divine_assert( 0 ); } // oh golly...

/******** stdio.h stubs */

extern "C" int sprintf(char *, const char *, ...) { __divine_assert( 0 ); return 0; }
extern "C" int snprintf(char *, size_t, const char *, ...) { __divine_assert( 0 ); return 0; }

/******** string.h stubs */

extern "C" int strcmp( const char *, const char * ) { __divine_assert( 0 ); return 0; }
extern "C" int strncmp( const char *, const char *, size_t ) { __divine_assert( 0 ); return 0; }
extern "C" int strlen( const char * ) { __divine_assert( 0 ); return 0; }
extern "C" char *strncpy( char *, const char *, size_t ) { __divine_assert( 0 ); return 0; }
extern "C" int memset() { __divine_assert( 0 ); return 0; }
extern "C" void *memcpy( void *, const void *, unsigned long ) { __divine_assert( 0 ); return 0; }

extern "C" int isalnum(int c) { __divine_assert( 0 ); return 0; }
extern "C" int isalpha(int c) { __divine_assert( 0 ); return 0; }
extern "C" int isascii(int c) { __divine_assert( 0 ); return 0; }
extern "C" int isblank(int c) { __divine_assert( 0 ); return 0; }
extern "C" int iscntrl(int c) { __divine_assert( 0 ); return 0; }
extern "C" int isdigit(int c) { __divine_assert( 0 ); return 0; }
extern "C" int isgraph(int c) { __divine_assert( 0 ); return 0; }
extern "C" int islower(int c) { __divine_assert( 0 ); return 0; }
extern "C" int isprint(int c) { __divine_assert( 0 ); return 0; }
extern "C" int ispunct(int c) { __divine_assert( 0 ); return 0; }
extern "C" int isspace(int c) { __divine_assert( 0 ); return 0; }
extern "C" int isupper(int c) { __divine_assert( 0 ); return 0; }
extern "C" int isxdigit(int c) { __divine_assert( 0 ); return 0; }

/******** unwind library stubs */

#include "libsupc++/unwind.h"

_Unwind_Reason_Code _Unwind_RaiseException (struct _Unwind_Exception *) { __divine_assert( 0 ); }
_Unwind_Reason_Code _Unwind_Resume_or_Rethrow (struct _Unwind_Exception *) { __divine_assert( 0 ); }
_Unwind_Reason_Code _Unwind_ForcedUnwind (struct _Unwind_Exception *, _Unwind_Stop_Fn, void *) { __divine_assert( 0 ); }
void _Unwind_DeleteException (struct _Unwind_Exception *) { __divine_assert( 0 ); }
_Unwind_Word _Unwind_GetGR (struct _Unwind_Context *, int) { __divine_assert( 0 ); }
void _Unwind_SetGR (struct _Unwind_Context *, int, _Unwind_Word) { __divine_assert( 0 ); }
_Unwind_Ptr _Unwind_GetIP (struct _Unwind_Context *) { __divine_assert( 0 ); }
_Unwind_Ptr _Unwind_GetIPInfo (struct _Unwind_Context *, int *) { __divine_assert( 0 ); }
void _Unwind_SetIP (struct _Unwind_Context *, _Unwind_Ptr) { __divine_assert( 0 ); }
_Unwind_Word _Unwind_GetCFA (struct _Unwind_Context *) { __divine_assert( 0 ); }
void *_Unwind_GetLanguageSpecificData (struct _Unwind_Context *) { __divine_assert( 0 ); }
_Unwind_Ptr _Unwind_GetRegionStart (struct _Unwind_Context *) { __divine_assert( 0 ); }
void _Unwind_SjLj_Register (struct SjLj_Function_Context *) { __divine_assert( 0 ); }
void _Unwind_SjLj_Unregister (struct SjLj_Function_Context *) { __divine_assert( 0 ); }
_Unwind_Word _Unwind_GetBSP (struct _Unwind_Context *) { __divine_assert( 0 ); }
_Unwind_Ptr _Unwind_GetDataRelBase (struct _Unwind_Context *) { __divine_assert( 0 ); }
_Unwind_Ptr _Unwind_GetTextRelBase (struct _Unwind_Context *) { __divine_assert( 0 ); }
void * _Unwind_FindEnclosingFunction (void *pc) { __divine_assert( 0 ); }
