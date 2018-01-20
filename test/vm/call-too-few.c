/* TAGS: min c */
int foo( int x, int y ) { return 0; }

int main() {
    int (*fun)( int ) = (int(*)( int ))foo;
    return fun( 0 ); /* ERROR */
}
