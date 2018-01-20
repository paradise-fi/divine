/* TAGS: min c */
int main() {
    int x;
    switch ( x ) { case 0: exit( 2 ); default: exit( 1 ); } /* ERROR */
    return 0;
}
