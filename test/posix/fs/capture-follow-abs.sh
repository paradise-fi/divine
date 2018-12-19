# TAGS: todo
. lib/testcase

cat > test.c <<EOF
/* VERIFY_OPTS: --capture capture/:follow:/ */
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

int main() {

    int fd = open( "file.txt", O_RDONLY );
    assert( fd >= 0 );
    assert( close( fd ) == 0 );

    return 0;
}
EOF

file=$(mktemp /tmp/capture.XXXXXX)
trap "rm -f $file" EXIT

mkdir capture/
ln -s $file capture/file.txt

verify test.c
