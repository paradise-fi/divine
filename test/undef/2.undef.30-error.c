int main() {
    int x;
    if ( x ? 0 : 42 ) /* ERROR */
        exit( 1 );
    return 0;
}
