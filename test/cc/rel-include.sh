# TAGS: min
. lib/testcase

mkdir -p 1/2/3

cat > 1/2/3/test.c <<EOF
#include "1.h"
#include "2.h"
#include "3.h"


int main() {
    return 0;
}
EOF

touch 1/2/3/3.h
touch 1/2/2.h
touch 1/1.h

cd 1/2/3
divine cc test.c -I.. -I../..
divine cc -c test.c -I.. -I../..
