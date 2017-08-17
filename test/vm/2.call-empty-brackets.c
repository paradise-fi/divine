/* CC_OPTS: $TESTS/vm/defs.c */
/* VERIFY_OPTS: -std=c89 */

int foo();

int main() {
    return foo( 0, 0, 42 );
}
