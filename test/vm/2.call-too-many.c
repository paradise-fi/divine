int foo( int x, int y ) { return x + y; }

int main() {
    int (*fun)( int, int, int ) = foo;
    return fun( 0, 0, 42 ); /* ERROR */
}
