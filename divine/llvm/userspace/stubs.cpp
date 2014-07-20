#include <cstdlib>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <stdarg.h>

#include <divine.h>
#include <_PDCLIB_locale.h>
#include <_PDCLIB_aux.h>

extern "C" {
    FILE *tmpfile() _PDCLIB_nothrow { __divine_assert( 0 ); return 0; }
    double atof( const char *r ) _PDCLIB_nothrow { __divine_problem( 9, 0 ); return 0; }
    double strtod( const char *, char ** ) _PDCLIB_nothrow { __divine_problem( 9, 0 ); return 0; }
    float strtof( const char *, char ** ) _PDCLIB_nothrow { __divine_problem( 9, 0 ); return 0; }
    long double strtold( const char *, char ** ) _PDCLIB_nothrow { __divine_problem( 9, 0 ); return 0; }
    size_t mbsrtowcs( wchar_t *, const char **, size_t, mbstate_t * ) { __divine_problem( 9, 0 ); return 0; }
    int wcscoll( const wchar_t *, const wchar_t *) { __divine_problem( 9, 0 ); return 0; }
    size_t wcsxfrm( wchar_t *, const wchar_t *, size_t ) { __divine_problem( 9, 0 ); return 0; }
    wint_t btowc( int ) { __divine_problem( 9, 0 ); return 0; }
    int wctob( wint_t ) { __divine_problem( 9, 0 ); return 0; }
    size_t wcsnrtombs( char *, const wchar_t **, size_t, size_t, mbstate_t * ) { __divine_problem( 9, 0 ); }
    size_t mbsnrtowcs( wchar_t *, const char **, size_t, size_t, mbstate_t * ) { __divine_problem( 9, 0 ); }
    int mbtowc( wchar_t *, const char *s, size_t ) {
        if ( !s )
            return 0; /* stateless */
        __divine_problem( 9, 0 );
    }
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

    char *getenv( const char * ) _PDCLIB_nothrow { __divine_problem( 9, 0 ); return 0; }

    void tzset() { __divine_problem( 9, 0 ); }
    int gettimeofday(struct timeval *, struct timezone *) { __divine_problem( 9, 0 ); return 0; }
    int settimeofday(const struct timeval *, const struct timezone *) { __divine_problem( 9, 0 ); return 0; }

    locale_t newlocale( int, const char *lc, locale_t ) {
        if ( strcmp( lc, "C" ) == 0 )
            return const_cast< locale_t >( &_PDCLIB_global_locale );

        __divine_problem( 9, 0 );
        return 0;
    }
}
