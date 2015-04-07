. lib

llvm_verify_cpp valid <<EOF
#include <cassert>
#include <new>

int global = 0;

struct x { virtual ~x() {} };
struct y : x { virtual ~y() { global = 1; } };

int main() {
    char mem[ 32 ];
    x *a = new (mem) y();
    y *b = dynamic_cast< y * >( a );
    delete a;
    assert( global == 1 );
    return 0;
}
EOF

llvm_verify_cpp valid <<EOF
#include <typeinfo>
#include <cassert>
struct x { virtual ~x() {} };

int main() {
    auto ti = &typeid( x );
    void **vtable = *(void***)ti;
    void **ti2 = vtable - 1;
    void **tiX = vtable - 2;
    assert( vtable[-1] );
    return 0;
}
EOF
