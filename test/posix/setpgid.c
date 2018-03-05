/* TAGS: fork min c */
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>

int main() {
    assert( getpgid( 0 ) == 1 );
    assert( setpgid( 0, -1 ) == -1 );
    assert( errno == EINVAL );

    assert( setpgid( 0, 2 ) == -1 );
    assert( errno == EPERM );

    pid_t pid = fork();

    if ( pid == 0 )
    {
        assert( setpgid( 2, 3 ) == -1 );
        assert( getpgid( 2 ) == 1 );
        assert( errno == EPERM );

        pid_t pid = fork();

        while( pid != 0 ){} // ensure child #3 is created

        if ( pid != 0 ) {
            assert( setpgid( 2, 0 ) == 0 );
            assert( getpgid( 2 ) == 2 );

            assert( setpgid( 3, 2 ) == 0 );
            assert( getpgid( 3 ) == 2 );
        }
    }
}
