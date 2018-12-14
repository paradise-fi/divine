/* TAGS: fork c */
#include <unistd.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

int main() {
    int original = umask( 066 );

    pid_t ret = fork();

    if ( ret == 0 )
        assert( umask( 077 ) == 066 );
    else
        assert( umask( 033 ) == 066 );

    return 0;
}
