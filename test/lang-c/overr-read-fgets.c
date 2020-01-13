/* TAGS: c */
/* VERIFY_OPTS: --stdin $SRC_DIR/stdin.txt -o nofail:malloc */
#include <stdio.h>
#include <string.h>
#include <assert.h>

int read()
{
    char str[20];
    char * ret = fgets( str, 20, stdin );
    assert( strcmp( str, "pac" ) == 0 );
    assert( strcmp( ret, "pac" ) == 0 );
    return 1;
}

int main()
{
    assert( read() == 1 );
}
