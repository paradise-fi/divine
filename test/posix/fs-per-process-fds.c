/* TAGS: fork min c */
#include <unistd.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

int main() {
    int fd = open( "file", O_CREAT | O_WRONLY, 0666 );

    pid_t ret = fork();

    if ( ret == 0 )
        assert( write( fd, "text\n",5 ) == 5 );
    else
        close( fd );

    return 0;
}
