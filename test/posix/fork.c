/* TAGS: fork min c */
#include <unistd.h>
#include <assert.h>

int main()
{
    pid_t pid = fork();

    if ( pid != 0 )
        assert( pid == 2 );
}
