#pragma once

#include <sys/divm.h>
#include <ctype.h>

void assume( int cond )
{
    if ( !cond )
        __vm_cancel();
}

/* Helper functions for constructing symbolic values
 * with specific properties (e.g. an arbitrary digit).
 * There are two variations: when EXPLICIT is defined,
 * the value is generated using __vm_choose (all
 * possibilities), otherwise symbolic values are used.
 * This is mostly because of (as of now) unsupported
 * operations on symbolic values. */

#ifdef EXPLICIT

char ascii( char begin, unsigned range )
{
    return begin + __vm_choose( range );
}

char digit() { return ascii( '0', 10 ); }
char lower() { return ascii( 'a', 26 ); }
char upper() { return ascii( 'A', 26 ); }

#else

char ascii( int (*pred)( int ) )
{
    char c = __sym_val_i8();
    assume( pred( c ) );
    return c;
}

char digit() { return ascii( isdigit ); }
char lower() { return ascii( islower ); }
char upper() { return ascii( isupper ); }

#endif
