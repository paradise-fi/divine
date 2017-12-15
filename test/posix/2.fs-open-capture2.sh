. lib/testcase

mkdir -p capture

cat > capture/fs-open-capture.c <<EOF
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

int main() {
	
	int fd = open( "file", O_WRONLY | O_CREAT | O_EXCL, 0644  );
    assert( fd == -1 );
    assert( errno == EEXIST );
    return 0;
}
EOF

mkdir capture/link
mkdir capture/link/dir
touch capture/link/file
ln capture/link/file capture/link/hardlinkFile

divine verify --threads 1 --capture capture/link:follow:/ capture/fs-open-capture.c
