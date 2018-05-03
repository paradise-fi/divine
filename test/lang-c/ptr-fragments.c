/* TAGS: min c */
#include <stdlib.h>
#include <assert.h>

int main()
{
    int a = 1234567;
    int *p1 = &a;
    char broken_frags[ 16 ];
    char frags[ 8 ];
    char wrong[ 8 ];
    int *p2 = NULL;

    for ( int i = 0; i < 8; ++i )
        broken_frags[ 2 * i ] = ( ( char * ) ( &p1 ) )[ i ];

    for ( int i = 0; i < 8; ++i )
        frags[ i ] = broken_frags[ 2 * i ];

    for ( int i = 0; i < 8; ++i )
        ( ( char * ) ( &p2 ) )[ i ] = frags[ i ];

    assert( p1 == p2 );
    assert( *p2 == a );
    assert( **( ( int ** ) frags ) == a );

    for ( int i = 0; i < 8; ++i )
        wrong[ i ] = frags[ i ];
    for ( int i = 0; i < 4; ++i )
        wrong[ i + 4 ] = frags[ 7 - i ];

    assert( **( ( int ** ) wrong ) == a ); /* ERROR */
}

