/* TAGS: fork min c */
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>

int main() {
    assert( getpgid( 0 ) == 1 );

    pid_t pid = fork();

    if ( pid == 0 )
        assert( getpgid( getpid() ) == 1 );
}
