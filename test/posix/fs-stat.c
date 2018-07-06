/* TAGS: c */
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

int main()
{
    int fd = open( "test", O_WRONLY | O_CREAT, 0644 );
    assert( fd >= 0 );
    struct stat st;
    stat( "test", &st );
    assert( ( st.st_mode & 0777 ) == 0644 );
    assert( st.st_nlink == 1 );
    return 0;
}
