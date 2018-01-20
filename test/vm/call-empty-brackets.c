/* TAGS: min c */
/* CC_OPTS: $TESTS/vm/call.helper.c */
/* VERIFY_OPTS: -std=c89 */

int foo();

int main() {
    return foo( 0, 0, 42 );
}
