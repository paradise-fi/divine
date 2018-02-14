# TAGS: min
. lib/testcase

cat > prog.c <<EOF
#include <assert.h>

int main()
{
    assert(1 != 1);
    return 0;
}
EOF

if ! divine verify prog.c | grep "Assertion failed: 1 != 1, file prog.c, line 5."
    then false;
fi
