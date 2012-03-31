#include "divine-llvm.h" 

int i = 1;
int *j = &i;

void * thread(void * in)
{
    if (in) {
        // depending on the interleaving, we may see 0, 1, or the free'd value (1024) here
        trace( "I am thread 1; i = %d, *j = %d", i, *j );
    } else {
        trace( "I am thread 0; i = %d, *j = %d", i, *j );
        i = 0;
    }
    return 0;
}

int main() {
    pthread_t t1,t2;
    pthread_create(&t1,0,thread,(void *)0);
    pthread_create(&t2,0,thread,(void *)1);
    return 0;
}
