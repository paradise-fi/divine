. lib/testcase

cat > src.cpp <<EOF
#include <sys/lamp.h>
#include <cassert>

int x;

int get() { return x; }

void foo()
{
    int val = __lamp_any_i32();
    if (val == 0) {
        val = get();
    } else {
        val -= 1;
    }
    int y = get();
    assert( x == y );
}

int main()
{
    foo();
}
EOF

sim --symbolic src.cpp <<EOF
> start
> break __lart_abstract.tobool.i1
> step --out
> step --out
> bitcode
> stepi
- FAULT
+ executing foo()
EOF
