/* TAGS: min c */
int array[2];
int main()
{
    memset( array, 1, 3 * sizeof( int ) ); /* ERROR */
    return 0;
}
