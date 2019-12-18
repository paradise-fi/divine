/* TAGS: c */
#define _GNU_SOURCE
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>

int main(void)
{
    char* line = NULL;
    size_t len = 0;
    errno = 0;
    int r = getline( &line, &len, stdin );
    assert( r == 0 || r == -1 );
    assert( r != -1 || (errno == 0 && "EOF") || (errno == ENOMEM) );
    free( line );
}

