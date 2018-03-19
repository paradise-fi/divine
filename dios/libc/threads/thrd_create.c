// (c) 2018 Vladimír Štill

#include <threads.h>

struct WrapThreadArg {
    thrd_start_t start;
    void *arg;
};

void *__c11_thread_start( void *_arg ) {
    struct WrapThreadArg arg = *(struct WrapThreadArg *)_arg;
    __vm_obj_free( _arg );
    int r = arg.start( arg.arg );
    return (void *)((uintptr_t) r);
}

int thrd_create( thrd_t *thr, thrd_start_t func, void *arg ) {
    struct WrapThreadArg *wrapArg = __vm_obj_make( sizeof( struct WrapThreadArg ) );
    wrapArg->start = func;
    wrapArg->arg = arg;
    if ( pthread_create( thr, 0, __c11_thread_start, wrapArg ) == 0 )
        return thrd_success;
    else
        return thrd_error; // todo: it seems pthread does not report OOM
}
