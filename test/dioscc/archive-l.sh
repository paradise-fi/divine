# TAGS:
. lib/testcase

cat > foo.h <<EOF
int foo();
EOF

cat > foo.c <<EOF
#include "foo.h"

int foo(){ return 6; }
EOF

dioscc -c foo.c
ar r libfoo.a foo.o
nm libfoo.a | grep "T foo"

cat > main.c <<EOF
#include <assert.h>
#include "foo.h"

int main()
{
    assert( foo() == 6 );
}
EOF

dioscc main.c libfoo.a
./a.out
divine check a.out
rm a.out

dioscc main.c -L. -lfoo
./a.out
divine check a.out
