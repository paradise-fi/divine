/* TAGS: fork min threads c */
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

int main ()
{
	pid_t pid;
	int ret;

	ret = pthread_atfork(NULL, NULL, NULL);
	if(ret != 0) return 0;

	pid = fork();
	
	if(pid < 0) return 1;

	if(pid == 0)
		pthread_exit(0);
}
