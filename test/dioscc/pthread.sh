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

dioscc prog.c -pthread

test -s a.out
test -x a.out

./a.out
divine check a.out


dioscc prog.c -lpthread -o lpthr

test -s lpthr
test -x lpthr

./lpthr
divine check lpthr
