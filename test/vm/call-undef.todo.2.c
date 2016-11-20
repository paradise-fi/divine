int main() {
    volatile int x = 4;
    int (*fun)( volatile int *, int );
    return fun( &x, 4 ); /* ERROR */
}
