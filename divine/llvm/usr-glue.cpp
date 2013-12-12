// -*- C++ -*- (c) 2013 Petr Rockai <me@mornfall.net>

#include <cstdlib>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#include <divine.h>

/*
 * Glue code that ties various bits of C and C++ runtime to the divine runtime
 * support. It's not particularly pretty. Other bits of this code are also
 * exploded over external/ which is even worse.
 */

/* Memory allocation */
void * malloc( size_t size ) throw() __attribute__((noinline)) {
    __divine_interrupt_mask();
    if ( __divine_choice( 2 ) )
        return __divine_malloc( size ); // success
    else
        return NULL; // failure
}

void *realloc( void *orig, size_t size ) throw() __attribute__((noinline)) {
    __divine_interrupt_mask();
    if ( size && __divine_choice( 2 ) ) {
        void *n = __divine_malloc( size );
        if ( orig ) {
            ::memcpy( n, orig, __divine_heap_object_size( orig ) );
            __divine_free( orig );
        }
        return n;
    } else
        return NULL; // failure
}

/* TODO malloc currently gives zeroed memory */
void *calloc( size_t n, size_t size ) throw() { return malloc( n * size ); }
void free( void * p) throw() { return __divine_free( p ); }

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

extern "C" void *dlsym( void *, void * ) { __divine_problem( 9, 0 ); return 0; }
extern "C" void *__errno_location() { __divine_problem( 9, 0 ); return 0; }

extern "C" { /* POSIX kernel APIs */

    void raise( int ) { __divine_problem( 9, 0 ); }
    int unlink( const char * ) { __divine_problem( 9, 0 ); return 0; }
    ssize_t read(int, void *, size_t) { __divine_problem( 9, 0 ); return 0; }
    ssize_t write(int, const void *, size_t) { __divine_problem( 9, 0 ); return 0; }
    off_t lseek(int, off_t, int) { __divine_problem( 9, 0 ); return 0; }
    int close(int) { __divine_problem( 9, 0 ); return 0; }

}

/* currently missing from our pdclib */
extern "C" FILE *tmpfile() throw() { __divine_assert( 0 ); return 0; }


extern "C" { /* pdclib glue functions */
    int _PDCLIB_rename( const char *, const char * ) { __divine_problem( 9, 0 ); return 0; }
    int _PDCLIB_open( const char *, int ) { __divine_problem( 9, 0 ); return 0; }
    void _PDCLIB_Exit( int rv ) {
        if ( rv )
            __divine_problem( 1, "exit called with non-zero value" );
        __divine_unwind( INT_MIN );
    }
}

extern "C" int nanosleep(const struct timespec *req, struct timespec *rem) {
    // I believe we will do nothing wrong if we verify nanosleep as NOOP,
    // it does not guearantee anything anyway
    return 0;
}

#include <stdarg.h>
#include <locale.h>

extern "C" {
    double atof( const char *r ) throw() { __divine_problem( 9, 0 ); return 0; }
    double strtod( const char *, char ** ) throw() { __divine_problem( 9, 0 ); return 0; }
    float strtof( const char *, char ** ) throw() { __divine_problem( 9, 0 ); return 0; }
    long double strtold( const char *, char ** ) throw() { __divine_problem( 9, 0 ); return 0; }
    size_t mbsrtowcs( wchar_t *, const char **, size_t, mbstate_t * ) { __divine_problem( 9, 0 ); return 0; }
    int wcscoll( const wchar_t *, const wchar_t *) { __divine_problem( 9, 0 ); return 0; }
    size_t wcsxfrm( wchar_t *, const wchar_t *, size_t ) { __divine_problem( 9, 0 ); return 0; }
    wint_t btowc( int ) { __divine_problem( 9, 0 ); return 0; }
    int wctob( wint_t ) { __divine_problem( 9, 0 ); return 0; }
    size_t wcsnrtombs( char *, const wchar_t **, size_t, size_t, mbstate_t * ) { __divine_problem( 9, 0 ); }
    size_t mbsnrtowcs( wchar_t *, const char **, size_t, size_t, mbstate_t * ) { __divine_problem( 9, 0 ); }
    int mbtowc( wchar_t *, const char *, size_t ) { __divine_problem( 9, 0 ); }
    int wctomb( char *, wchar_t ) { __divine_problem( 9, 0 ); }
    size_t mbrlen( const char *, size_t, mbstate_t * ) { __divine_problem( 9, 0 ); }
    wchar_t *wmemset( wchar_t *, wchar_t, size_t ) { __divine_problem( 9, 0 ); }
    long wcstol( const wchar_t *, wchar_t **, int ) { __divine_problem( 9, 0 ); return 0; }
    long long wcstoll( const wchar_t *, wchar_t **, int ) { __divine_problem( 9, 0 ); return 0; }
    unsigned long wcstoul( const wchar_t *, wchar_t **, int ) { __divine_problem( 9, 0 ); return 0; }
    unsigned long long wcstoull( const wchar_t *, wchar_t **, int ) { __divine_problem( 9, 0 ); return 0; }
    double wcstod( const wchar_t *, wchar_t ** ) { __divine_problem( 9, 0 ); return 0; }
    float wcstof( const wchar_t *, wchar_t ** ) { __divine_problem( 9, 0 ); return 0; }
    long double wcstold( const wchar_t *, wchar_t ** ) { __divine_problem( 9, 0 ); return 0; }

    int wprintf( const wchar_t *, ... ) { __divine_problem( 9, 0 ); return 0; }
    int fwprintf( FILE *, const wchar_t *, ... ) { __divine_problem( 9, 0 ); return 0; }
    int swprintf( wchar_t *, size_t, const wchar_t *, ... ) { __divine_problem( 9, 0 ); return 0; }

    int vwprintf( const wchar_t *, va_list ) { __divine_problem( 9, 0 ); return 0; }
    int vfwprintf( FILE *, const wchar_t *, va_list ) { __divine_problem( 9, 0 ); return 0; }
    int vswprintf( wchar_t *, size_t, const wchar_t *, va_list ) { __divine_problem( 9, 0 ); return 0; }

    char *getenv( const char * ) { __divine_problem( 9, 0 ); return 0; }

    void tzset() { __divine_problem( 9, 0 ); }
    int gettimeofday(struct timeval *, struct timezone *) { __divine_problem( 9, 0 ); return 0; }
    int settimeofday(const struct timeval *, const struct timezone *) { __divine_problem( 9, 0 ); return 0; }

    locale_t newlocale( int, const char *, locale_t ) { __divine_problem( 9, 0 ); return 0; }

    int main(...);
    struct global_ctor { int prio; void (*fn)(); };

    void _divine_start( int ctorcount, void *ctors, int mainproto )
    {
        int r;
        global_ctor *c = reinterpret_cast< global_ctor * >( ctors );
        for ( int i = 0; i < ctorcount; ++i ) {
            c->fn();
            c ++;
        }
        switch (mainproto) {
            case 0: r = main(); break;
            case 1: r = main( 0 ); break;
            case 2: r = main( 0, 0 ); break;
            case 3: r = main( 0, 0, 0 ); break;
            default: __divine_problem( 1, "don't know how to run main()" );
        }
        exit( r );
    }
}
