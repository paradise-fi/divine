/* VERIFY_OPTS: -C,-include,inc.h -C,-I`dirname $1` */
/* SKIP_CC: 1 */

int main() {
    return foo();
}
