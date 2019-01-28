/* TAGS: min c */

int main() {
    int val;
    int old = 0;
    int new = 1;
    __sync_bool_compare_and_swap( &val, old, new ); /* ERROR */
}
