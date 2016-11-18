. lib/testcase

cat >> cfile <<EOF
#include <assert.h>

void foo( void );

int main() {
    foo();
    assert( sizeof( 'a' ) == sizeof( int ) ); // in C, characters are int
}
EOF

cat >> cppfile <<EOF
extern "C" void foo() { } // extern "C" does not work in C
EOF

divine cc -x c cfile -x c++ cppfile -o test.bc
divine verify test.bc | tee verify.out
check verify "$0"
