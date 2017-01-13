int main() {
    int (*fun)( int ) = 0;
    return fun( 0 ); /* ERROR */
}
