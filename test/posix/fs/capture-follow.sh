. lib/testcase

cat > test.c <<EOF
/* VERIFY_OPTS: --capture capture/:follow:/subdir */
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

int main() {

    int fd = open( "subdir/file.txt", O_RDONLY );
    assert( fd >= 0 );
    assert( close( fd ) == 0 );

    return 0;
}
EOF

touch file
mkdir capture/
ln -s ../file capture/file.txt

verify test.c
