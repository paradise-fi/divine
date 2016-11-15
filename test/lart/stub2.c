/* VERIFY_OPTS: --lart stubs */

void foo();
int baz();

int main( int argc, char **argv ) {
    if ( argc > 12000 )
        foo();
    return baz(); /* ERROR */
}
