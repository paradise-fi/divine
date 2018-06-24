/* getenv( const char * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

/* This is an example implementation of getenv() fit for use with POSIX kernels.
*/

#include <string.h>
#include <stdlib.h>

#ifndef REGTEST

extern char ** environ;

char * getenv( const char * name )
{
    if ( !name || !environ )
        return NULL;

    size_t len = strlen( name );
    size_t index = 0;
    while ( environ[ index ] != NULL )
    {
        if ( strncmp( environ[ index ], name, len ) == 0 && environ[ index ][ len ] == '=' )
        {
            return environ[ index ] + len + 1;
        }
        index++;
    }
    return NULL;
}

char * __findenv( const char *name, int len, int *offset )
{
	int i;
	const char *np;
	char **p, *cp;

	if (name == NULL || environ == NULL)
		return (NULL);
	for (p = environ + *offset; (cp = *p) != NULL; ++p) {
		for (np = name, i = len; i && *cp; i--)
			if (*cp++ != *np++)
				break;
		if (i == 0 && *cp++ == '=') {
			*offset = p - environ;
			return (cp);
		}
	}
	return (NULL);
}

#endif

#ifdef TEST
#include "_PDCLIB_test.h"

int main( void )
{
    TESTCASE( strcmp( getenv( "SHELL" ), "/bin/bash" ) == 0 );
    /* TESTCASE( strcmp( getenv( "SHELL" ), "/bin/sh" ) == 0 ); */
    return TEST_RESULTS;
}

#endif
