/* TAGS: min c */
/* VERIFY_OPTS: --lart abstrubs --lamp trivial */
/* EXPECT: --result error --symbol _Exit */

int foo();
int baz();

int main( int argc, char **argv ) {
    if ( argc > 12000 )
        foo();
    return baz() ? 1 : 0;
}
