/* TAGS: fork min c */
#include <unistd.h>
#include <assert.h>

int x = 1;

int main()
{
    pid_t pid = fork();

    if ( pid != 0 )
        assert( x == 1 );
    else
    {
        x = 2;
        assert( x == 2 );
    }
}
