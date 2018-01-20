/* TAGS: min c */
/* VERIFY_OPTS: --lart stubs */

void foo();
int baz() { return 0; }

int main( int argc, char **argv ) {
    if ( argc > 12000 )
        foo();
    return baz();
}
