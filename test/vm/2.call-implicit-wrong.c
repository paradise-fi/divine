/* CC_OPTS: $TESTS/vm/defs.c */
/* VERIFY_OPTS: -std=c89 */

int main() {
    long y = 40;
    return baz( 2, y ) - 42; /* ERROR */ /* return value of baz is long */
}
