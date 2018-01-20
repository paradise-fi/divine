#include <sys/stat.h>
#include <sys/types.h>
#include <assert.h>
#include <errno.h>


__attribute__((constructor))
static void __mkdir_testing() {
    int r = mkdir( "testing", 0777 );
    assert( r == 0 );
    r = mkdir( "/tmp", 01777 );
    assert( r == 0 || errno == EEXIST );
}

