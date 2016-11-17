. lib/testcase

cat > testcase.c <<EOF
#include "foo.h"
int main() {}
EOF

mkdir 1
touch 1/foo.h

divine verify -C,-I,$PWD/1 testcase.c
divine verify -C,-I$PWD/1 testcase.c
