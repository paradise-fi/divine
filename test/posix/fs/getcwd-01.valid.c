/* TAGS: min c */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include <sys/trace.h>

int main()
{
	mkdir("divine", 0777);
	char workingDir[100];
	char *res = getcwd(workingDir, 99);
  __dios_trace_f( "cwd: %s", workingDir );
  __dios_trace_f( "res: %s", res );

	assert ( res == workingDir );
	assert( strcmp(workingDir, "/" ) == 0 );
	chdir("divine");
	res = getcwd(workingDir, 99);
	assert ( res == workingDir );
	assert( strcmp(workingDir, "/divine" ) == 0 );
   	return 0;
}
