// (c) 2018 Vladimír Štill

#include <threads.h>

void thrd_yield() {
    pthread_yield();
}
