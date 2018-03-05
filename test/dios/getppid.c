/* TAGS: fork min c */
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>

int main() {
    assert( getppid() == 0 );
    pid_t parent = getpid();

    pid_t pid = fork();

    if ( pid == 0 )
        assert( getppid() == parent );
}
