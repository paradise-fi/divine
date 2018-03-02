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
