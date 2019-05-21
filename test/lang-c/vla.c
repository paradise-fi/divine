/* TAGS: min c lifetime */
int main( int argc, char **argv ) {
    int *x;
    {
        int vla[argc];
        x = vla;
    }
    *x = 1; /* ERROR */
    return 0;
}
