/* TAGS: c */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/trace.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

int main()
{
	assert( mkdir("hajdeby", 0777) == 0 );
	assert( mkdir("fixa", 0777) == 0 );
	assert( chdir("hajdeby") == 0 );
	assert( mkdir("fixa", 0777) == 0 );
	char workingDir[100];
	char *res;

	res = getcwd(workingDir, 99);
	assert ( res == workingDir );
	assert( strcmp(workingDir, "/hajdeby" ) == 0 );
	chdir("fixa");
	res = getcwd(workingDir, 99);
	assert( strcmp(workingDir, "/hajdeby/fixa" ) == 0 );
	chdir("..");
	res = getcwd(workingDir, 99);
	assert( strcmp(workingDir, "/hajdeby" ) == 0 );
   	return 0;
}
