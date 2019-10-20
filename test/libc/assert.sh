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

if ! divine verify prog.c | fgrep "prog.c:5: int main(): assertion '1 != 1' failed"
    then false;
fi
