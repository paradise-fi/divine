# TAGS: min
. lib/testcase

mkdir 1
touch 1/foo.h
cat > test.c <<EOF
#include "foo.h"
int main() {}
EOF

touch 1/foo.h
divine cc test.c -I1
divine cc test.c -I$PWD/1
divine cc test.c -C,-I$PWD/1
divine exec -C,-I$PWD/1 test.c

divine cc -c test.c -I$PWD/1
divine cc test.c -c -I$PWD/1
divine cc -I$PWD/1 -c test.c
divine cc test.c -I$PWD/1 -c
divine cc -c -I$PWD/1 test.c
divine cc -I$PWD/1 test.c -c
