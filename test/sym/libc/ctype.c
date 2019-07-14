/* TAGS: sym c todo */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

#include <rst/domains.h>
#include <ctype.h>
#include <assert.h>

int main()
{
    unsigned char x = __sym_val_i8();

    // 0-9
    if ( x >= 48 && x <= 57 )
    {
        assert( isdigit( x ) );
        assert( isxdigit( x ) );
        assert( isalnum( x ) );
    }
    else
        assert( !isdigit( x ) );

    // space
    if ( x == 32 )
    {
        assert( isspace( x ) );
        assert( isblank( x ) );
        assert( tolower( x ) == x );
        assert( toupper( x ) == x );
    }

    // a-z
    if ( x >= 97 && x <= 122 )
    {
        assert( islower( x ) );
        assert( isalnum( x ) );
        assert( toupper( x ) == x - 32 );
        assert( tolower( x ) == x );
    }
    else
        assert( !islower( x ) );

    // A-Z
    if ( x >= 65 && x <= 90 )
    {
        assert( toupper( x ) == x );
        assert( tolower( x ) == x + 32 );
    }

    // a-z || A-Z
    if ( ( x >= 97 && x <= 122 ) ||
         ( x >= 65 && x <= 90 ) )
    {
        assert( isalpha( x ) );
        assert( islower( x ) || isupper( x ) );
        assert( isgraph( x ) );
    }

    // A || !
    if ( x == 65 || x == 33 )
    {
        assert( ( isalpha( x ) && isupper( x ) ) || ispunct( x ) );
    }

    // we know nothing about the value
    assert( iscntrl( x ) || !iscntrl( x ) );
    assert( isgraph( x ) || !isgraph( x ) );
}
