/* TAGS: fork min threads c */
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

int prep_val;
int parent_val;
int child_val;

static void prepare_handler()
{
	prep_val = 1;
	return;
}

static void parent_handler()
{
	parent_val = 1;
	return;
}

static void child_handler()
{
	child_val = 1;
	return;
}

int main ()
{
	pid_t pid;

	prep_val = parent_val = child_val = 0;

	if( pthread_atfork( prepare_handler, parent_handler, child_handler ) != 0 )
		return 0; // ignore failure

	pid = fork();

	if( pid == 0 )
    {
        assert( child_val );
		pthread_exit( 0 );
    }
    else
        assert( parent_val );

    assert( prep_val );
}

