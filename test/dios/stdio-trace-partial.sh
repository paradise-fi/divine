# TAGS:
. lib/testcase

cat > test.c <<EOF
#include <stdio.h>
#include <assert.h>
int main()
{
    fputs( "incomplete", stdout );
    assert( 0 );
}
EOF

divine verify test.c | tee report.txt
fgrep '  [0] incomplete' report.txt
