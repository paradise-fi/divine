/* TAGS c */
/* VERIFY_OPTS: -o nofail:malloc */
#include <stdio_ext.h>
#include <assert.h>

int main()
{
    FILE* fp = fopen("file.txt", "w");
    assert( fp != NULL );
    assert( __fpending(fp) == 0U );
    assert( fputc('x', fp) == 'x' );

    assert( __fpending(fp) == 1U );
    assert( fputc('y', fp) == 'y' );

    assert( __fpending(fp) == 2U );
    fflush( fp );
    assert( __fpending(fp) == 0U );
    fclose( fp );
}
