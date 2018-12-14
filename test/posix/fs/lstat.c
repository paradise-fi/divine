/* TAGS: c */
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/trace.h>

int main()
{
    int fd = open( "test", O_WRONLY | O_CREAT, 0644 );
    assert( fd >= 0 );
    assert( symlink( "test", "sym" ) == 0 );
    struct stat st;

    lstat( "test", &st );
    assert( ( st.st_mode & S_IFMT ) == S_IFREG );
    assert( ( st.st_mode & 0777 ) == 0644 );
    assert( st.st_nlink == 1 );

    stat( "sym", &st );
    assert( ( st.st_mode & S_IFMT ) == S_IFREG );
    assert( ( st.st_mode & 0777 ) == 0644 );
    assert( st.st_nlink == 1 );

    lstat( "sym", &st );
    __dios_trace_f( "mode = %o", st.st_mode );
    assert( ( st.st_mode & S_IFMT ) == S_IFLNK );
    assert( ( st.st_mode & 0777 ) == 0777 );
    assert( st.st_nlink == 1 );

    return 0;
}
