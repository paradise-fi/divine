/* TAGS: min c */
/* CC_OPTS: $TESTS/vm/call.helper.c */
/* VERIFY_OPTS: -std=c89 */

int main() {
    long y = 40;
    return bar( 2, y ) - 42;
}
