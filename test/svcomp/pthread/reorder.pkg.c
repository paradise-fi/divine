/* TAGS: c threads */
/* CC_OPTS: */
/* VERIFY_OPTS: -o nofail:malloc */

#include <assert.h>
#include <stdlib.h>
#include <pthread.h>

// both instances are in svcomp
// V: 2-2 CC_OPT: -DSET=2 -DCHECK=2
// V: 4-1 CC_OPT: -DSET=4 -DCHECK=1

static int a = 0;
static int b = 0;

void *setThread(void *param);
void *checkThread(void *param);
void set();
int check();

int main() {
    int i, err;

    pthread_t setPool[SET];
    pthread_t checkPool[CHECK];

    for (i = 0; i < SET; i++) {
        if (0 != (err = pthread_create(&setPool[i], NULL, &setThread, NULL))) {
            exit(-1);
        }
    }

    for (i = 0; i < CHECK; i++) {
        if (0 != (err = pthread_create(&checkPool[i], NULL, &checkThread,
                                       NULL))) {
            exit(-1);
        }
    }

    for (i = 0; i < SET; i++) {
        if (0 != (err = pthread_join(setPool[i], NULL))) {
            exit(-1);
        }
    }

    for (i = 0; i < CHECK; i++) {
        if (0 != (err = pthread_join(checkPool[i], NULL))) {
            exit(-1);
        }
    }

    return 0;
}

void *setThread(void *param) {
    a = 1;
    b = -1;

    return NULL;
}

void *checkThread(void *param) {
    assert((a == 0 && b == 0) || (a == 1 && b == -1)); /* ERROR */
    return NULL;
}

