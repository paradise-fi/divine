/* TAGS: min c */

int main() {
    int val = 0;
    int old;
    int new = 1;
    __sync_bool_compare_and_swap( &val, old, new ); /* ERROR */
}
