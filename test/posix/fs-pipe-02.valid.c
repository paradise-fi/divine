/* TAGS: c */
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

int main() {
    assert( mkfifo( "pipe", 0644 ) == 0 );

    int fd = open( "pipe", O_WRONLY );
    assert( 0 );
    return 0;
}
