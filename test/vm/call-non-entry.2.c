#include <stdint.h>

int foo( int x ) {
    return x + 42;
}

int main() {
    int (*fun)( int ) = (int (*)( int ))(((uintptr_t)foo) + 1);
    return fun( -42 ); /* ERROR */
}

