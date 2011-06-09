int main() {
    int i = 1;
    int *j = &i;
    int thread = thread_create();
    if (thread) {
        // depending on the interleaving, we may see 0, 1, or the free'd value (1024) here
        trace( "I am thread 1; i = %d, *j = %d", i, *j );
    } else {
        trace( "I am thread 0; i = %d, *j = %d", i, *j );
        i = 0;
    }
}
