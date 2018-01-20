/* TAGS: min c */
/* VERIFY_OPTS: -C,-D"RETURN(x)=(x-2)" */
/* SKIP_CC: 1 */

int main() {
    return RETURN(2);
}
