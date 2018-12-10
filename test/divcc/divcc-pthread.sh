# TAGS:
. lib/testcase

cat > prog.c <<EOF
#include <pthread.h>

void *f(void * i) {
    return NULL;
}

int main()
{
    pthread_t thr;
    pthread_create(&thr, NULL, f, NULL);
}
EOF

divcc prog.c -pthread

if ! [ -s a.out ] || ! [ -x a.out ];
    then false;
fi

./a.out
divine check a.out
