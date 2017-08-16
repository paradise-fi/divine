#include <pthread.h>

int main() {
    pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_unlock( &mtx ); /* ERROR */
}
