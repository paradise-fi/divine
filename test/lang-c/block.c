/* TAGS: min c */

int main(void) {
    int* p;
    {
        int a[ 10 ];
        p = a;
    }
    p[ 0 ] = 1; /* ERROR */
}
