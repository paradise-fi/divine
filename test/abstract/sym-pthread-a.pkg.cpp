/* TAGS: sym c++ todo */
/* VERIFY_OPTS: --symbolic --svcomp */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
// V: v.O1 CC_OPT: -O1
// V: v.O2 CC_OPT: -O2
// V: v.Os CC_OPT: -Os
//
extern "C" int __VERIFIER_nondet_int(void);

#include <pthread.h>
#include <stdio.h>
#include <assert.h>


struct Value {
    bool has_value() const { return has_data; }

    void set( int val ) {
        data = val;
        has_data = true;
    }

    int get() {
        assert( has_data );
        return data;
    }

private:
    int data;
    bool has_data = false;
};

Value global;
pthread_mutex_t m;

void * t1( void * ) {
    pthread_mutex_lock(&m);
    int val =  __VERIFIER_nondet_int();
    global.set( val );
    pthread_mutex_unlock(&m);
    return nullptr;
}

void * t2( void * ) {
    pthread_mutex_lock(&m);
    if ( global.has_value() )
        assert( global.get() != 0 ); /* ERROR */
    pthread_mutex_unlock(&m);
    return nullptr;
}

int main( void ) {
    pthread_t id1, id2;
    pthread_mutex_init(&m, 0);

    pthread_create( &id1, nullptr, t1, nullptr );
    pthread_create( &id2, nullptr, t2, nullptr );

    pthread_join(id1, nullptr);
    pthread_join(id2, nullptr);

    return 0;
}

