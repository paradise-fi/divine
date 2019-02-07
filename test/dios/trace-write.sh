# TAGS:
. lib/testcase

cat > test.c <<EOF
#include <unistd.h>
#include <assert.h>

int main()
{
    write( 1, "+stdout\n", 8 );
    write( 2, "+stderr\n", 8 );
    assert( 0 );
}
EOF

divine verify test.c | tee report.txt
fgrep '  [0] +stdout' report.txt
fgrep '  [0] +stderr' report.txt
