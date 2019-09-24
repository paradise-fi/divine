/* TAGS: min sym c */
/* VERIFY_OPTS: --lart abstrubs --symbolic */
/* ERRSPEC: error trace: Non-zero exit code: 1 */

int foo();
int baz();

int main( int argc, char **argv ) {
    if ( argc > 12000 )
        foo();
    return baz() ? 1 : 0;
}
