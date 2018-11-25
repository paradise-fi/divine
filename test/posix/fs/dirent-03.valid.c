/* TAGS: c */
#include <assert.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/trace.h>

int main() {
    assert( mkdir( "dir", 0644 ) == 0 );
    errno = 0;

    DIR *d = opendir( "dir" );
    assert( d != NULL );
    closedir( d );

    DIR *e = opendir( "dir/x" );
    assert( e == NULL );
    __dios_trace_f( "errno = %d", errno );
    assert( errno == EACCES );
    closedir( e );

    return 0;
}
