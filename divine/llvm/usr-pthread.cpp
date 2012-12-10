/*
   TODO:
         - consider volatile, uint32_t, const and static

         --- FUTURE ---
         - all remaining (but meaningfull for explicit model checking) pthread attributes
         - pthread_cancel and related, rwlocks, barrier
  */


/* Macros */
#ifdef DEBUG
#define DBG_ASSERT(x) assert(x);
#else
#define DBG_ASSERT(x)
#endif
#define BREAK_MASK() do { __divine_interrupt_unmask();    \
                          /* __divine_interrupt(); ? */   \
                          __divine_interrupt_mask(); } while(0);

// LTID = local thread ID - unique for thread lifetime (returned by __divine_get_tid)
// GTID = global thread ID - unique during the entire execution
// PTID = pthread_t = GTID (16b) + LTID (16b)
#define GTID(PTID)       (PTID >> 16)
#define LTID(PTID)       (PTID & 0xFFFF)
#define PTID(GTID,LTID)  ((GTID << 16) | LTID)

/* Includes */
#define PROTOTYPES_ONLY
#include "usr.h"
#undef PROTOTYPES_ONLY
#include "pthread.h"
#include <setjmp.h>
#include <errno.h>

/* TEMPORARY */
#ifdef NO_BOOL
#define bool int
#endif
#ifdef NEW_INTERP_BUGS
void _ZSt9terminatev( void ) {}
#endif

/* Internal data types */
struct Entry {
    void *(*entry)(void *);
    void *arg;
    bool initialized;
};

struct Thread { // (user-space) information maintained for every (running) thread
    // global thread ID
    int gtid;

    // join & detach
    bool running;
    bool detached;
    void *result;

    // exit -> back to the entry wrapper
    jmp_buf entry_buf;

    // conditional variables
    bool sleeping;
    pthread_cond_t* condition;
};

/* Internal globals*/
bool initialized = false;
unsigned alloc_pslots = 0; // num. of pointers (not actuall slots) allocated
unsigned thread_counter = 1;
Thread ** threads = NULL;
pthread_key_t keys = NULL;

/* Helper functions */
template< typename T >
T* realloc(T* old_ptr, unsigned old_count, unsigned new_count) {
    T* new_ptr = static_cast< T* >( __divine_malloc( sizeof(T) * new_count ) );
    if (old_ptr) {
#if defined(NO_INTRINSIC_MEMCPY) || defined(NEW_INTERP_BUGS)
        for (int i = 0; i < (old_count < new_count ? old_count : new_count); i++)
            new_ptr[i] = old_ptr[i]; // only basic types are copied
#else
        memcpy(static_cast< void* >(&new_ptr), static_cast< void* >(&old_ptr),
               sizeof(T) * (old_count < new_count ? old_count : new_count));
#endif
        __divine_free( old_ptr );
    }    
    return new_ptr;
}

/* Basic functions */
int _get_gtid(const int ltid) {
#ifndef NEW_INTERP_BUGS
    DBG_ASSERT((ltid >= 0) && (ltid < alloc_pslots))
#endif
    if (threads[ltid] != NULL)
        return threads[ltid]->gtid;
    else
        return -1; // invalid gtid
}

void _init_thread(const int gtid, const int ltid, const pthread_attr_t attr) {
    DBG_ASSERT(ltid >= 0 && gtid >= 0)
    pthread_key_t key;

    // reallocate thread local storage if neccessary
    if (ltid >= alloc_pslots) {
        DBG_ASSERT(ltid == alloc_pslots) // shouldn't skip unallocated slots
        int new_count = ltid + 1;

        // thread metadata
        threads = realloc< Thread* >(threads, alloc_pslots, new_count);
        threads[ltid] = NULL;

        // TLS
        key = keys;
        while (key) {
            key->data = realloc< void* >(key->data, alloc_pslots, new_count);
            key = key->next;
        }
        alloc_pslots = new_count;
    }

    // allocate slot for thread metadata
    DBG_ASSERT(threads[ltid] == NULL)
    threads[ltid] = static_cast< Thread* >( __divine_malloc(sizeof(Thread)) );
    DBG_ASSERT(threads[ltid] != NULL)

    // initialize thread metadata
    Thread* thread = threads[ltid];
    DBG_ASSERT(thread != NULL)
    thread->gtid = gtid;
    thread->running = false;
    thread->detached = (attr == PTHREAD_CREATE_DETACHED);
    thread->sleeping = false;
    thread->result = NULL;
    thread->condition = NULL;

    // associate value NULL with all defined keys for this new thread
    key = keys;
    while (key) {
        key->data[ltid] = NULL;
        key = key->next;
    }
}

void _initialize(void) {
    if (!initialized) {        
        // initialize implicitly created main thread
        DBG_ASSERT(alloc_pslots == 0)
        _init_thread(0, 0, PTHREAD_CREATE_DETACHED);
        threads[0]->running = true;

        // etc... more initialization steps might come here

        initialized = true;
    }
}

void _pthread_entry(void *_args)
{
    __divine_interrupt_mask();
    Entry *args = static_cast< Entry* >(_args);
    int ltid = __divine_get_tid();
    DBG_ASSERT(ltid < alloc_pslots)
    Thread* thread = threads[ltid];
    DBG_ASSERT(thread != NULL)
    pthread_key_t key;

    // copy arguments
    void * arg = args->arg;
    void *(*entry)(void *) = args->entry;

    // parent may delete args and leave pthread_create now
    args->initialized = true;

    // call entry function
    thread->running = true;
#ifndef NO_JMP
    if ( !setjmp(thread->entry_buf) ) {
#endif
        __divine_interrupt_unmask();
        // from now on, args and _args should not be accessed

        // __divine_interrupt(); ?

        thread->result = entry(arg);
#ifndef NO_JMP
    } // else exited by pthread_exit
#endif

    __divine_interrupt_mask();

    DBG_ASSERT(thread->sleeping == false)

    // all thread specific data destructors are run
    key = keys;
    while (key) {
        void* data = pthread_getspecific(key);
        if (data) {
            pthread_setspecific(key, NULL);
            if (key->destructor) {
                int iter = 0;
                while (data && iter < PTHREAD_DESTRUCTOR_ITERATIONS) {
                    (key->destructor)(data);
                    data = pthread_getspecific(key);
                    pthread_setspecific(key, NULL);
                    ++iter;
                }
            }
        }
        key = key->next;
    }

    thread->running = false;

    // wait until detach / join
    while (!thread->detached)
        BREAK_MASK()

    // cleanup
    __divine_free(thread);
    threads[ltid] = NULL;
}

int pthread_create(pthread_t *ptid, const pthread_attr_t *attr, void *(*entry)(void *),
                   void *arg) {
    __divine_interrupt_mask();
    _initialize();
    DBG_ASSERT(alloc_pslots > 0)

    // test input arguments
    if (ptid == NULL || entry == NULL)
        return EINVAL;

    // create new thread and pass arguments to the entry wrapper
    Entry* args = static_cast<Entry*>( __divine_malloc(sizeof(Entry)) );
    args->entry = entry;
    args->arg = arg;
    args->initialized = false;
    int ltid = __divine_new_thread(_pthread_entry, static_cast<void*>(args));
    DBG_ASSERT(ltid >= 0)

    // generate a unique ID
    int gtid = thread_counter++;    
    // 65536 is in fact the maximum number of threads (used during the entire execution)
    // we can handle (capacity of pthread_t is the limiting factor).
    __divine_assert(thread_counter <= (1 << 16));
    *ptid = PTID(gtid, ltid);

    // thread initialization
    _init_thread(gtid, ltid, (attr == NULL ? PTHREAD_CREATE_JOINABLE : *attr));
    DBG_ASSERT(ltid < alloc_pslots)

    while (!args->initialized)  // wait, do not free args yet
        BREAK_MASK()

    // cleanup and return
    __divine_free(args);
    return 0;
}

void pthread_exit(void *result) {
    __divine_interrupt_mask();
    _initialize();

    int ltid = __divine_get_tid();
    DBG_ASSERT(ltid < alloc_pslots);
    int gtid = _get_gtid(ltid);
    DBG_ASSERT(gtid >= 0 && gtid < thread_counter)

    threads[ltid]->result = result;

    if (gtid == 0) {
        // join every other thread and exit
        for (int i = 1; i < alloc_pslots; i++) {
            while (threads[i] && threads[i]->running)
                BREAK_MASK()
        }

        // FIXME: how to return from main()?

    } else {
        // return to _pthread_entry
#ifndef NO_JMP
        longjmp(threads[ltid]->entry_buf, 1);
#endif
    }
}

int pthread_join(pthread_t ptid, void **result) {
    __divine_interrupt_mask();

    int ltid = LTID(ptid);
    int gtid = GTID(ptid);

    if ( (ltid < 0) || (ltid >= alloc_pslots) ||
         (gtid < 0) || (gtid >= thread_counter))
        return EINVAL;

    if (gtid != _get_gtid(ltid))
        return ESRCH;

    if (gtid == _get_gtid(__divine_get_tid()))
        return EDEADLK;

    if (threads[ltid]->detached)
        return EINVAL;

    // wait for the thread to finnish
    while (threads[ltid]->running)
        BREAK_MASK()

    if ( (gtid != _get_gtid(ltid)) ||
         (threads[ltid]->detached) ) {
        // meanwhile detached
        return EINVAL;
    }

    // copy result
    if (result)
        *result = threads[ltid]->result;

    // let the thread to terminate now
    threads[ltid]->detached = true;
    return 0;
}

int pthread_detach(pthread_t ptid) {
    __divine_interrupt_mask();

    int ltid = LTID(ptid);
    int gtid = GTID(ptid);

    if ( (ltid < 0) || (ltid >= alloc_pslots) ||
         (gtid < 0) || (gtid >= thread_counter))
        return EINVAL;

    if (gtid != _get_gtid(ltid))
        return ESRCH;

    if (threads[ltid]->detached)
        return EINVAL;

    threads[ltid]->detached = true;
    return 0;
}

pthread_t pthread_self(void) {
    __divine_interrupt_mask();
    int ltid = __divine_get_tid();
    return PTID(_get_gtid(ltid), ltid);
}

/* Thread attributes */
int pthread_attr_destroy(pthread_attr_t *) {
    return 0;
}

int pthread_attr_init(pthread_attr_t *attr) {
    *attr = 0;
    return 0;
}

/* Mutex */

/*
  pthread_mutex_t representation:
  -----------------------------------------------------------------------------------------------
 | *free* | initialized?: 1 bit | type: 2 bits | lock counter: 8 bits | (thread_id + 1): 16 bits |
  -----------------------------------------------------------------------------------------------
  */

int _mutex_adjust_count( pthread_mutex_t *mutex, int adj ) {
    int count = ((*mutex) & 0xFF0000) >> 16;
    count += adj;
    if (count >= (1 << 8))
        return EAGAIN;

    (*mutex) &= ~0xFF0000;
    (*mutex) |= count << 16;
    return 0;
}

int _mutex_lock(pthread_mutex_t *mutex, bool wait) {
    int gtid = _get_gtid(__divine_get_tid());
    DBG_ASSERT(gtid >= 0 && gtid < thread_counter)

    if ( mutex == NULL || !((*mutex) & _INITIALIZED_MUTEX) ) {
        return EINVAL; // mutex does not refer to an initialized mutex object
    }

    if ( ((*mutex) & 0xFFFF) == gtid + 1 ) {
        // already locked by this thread
        DBG_ASSERT((*mutex) & 0xFF0000) // count should be > 0
        if ( !((*mutex) & PTHREAD_MUTEX_RECURSIVE) ) {
            if ((*mutex) & PTHREAD_MUTEX_ERRORCHECK)
                return EDEADLK;
            else
                __divine_assert( 0 );
        }
    }

    while (((*mutex) & 0xFFFF) && ((*mutex) & 0xFFFF) != (gtid + 1)) {
        DBG_ASSERT((*mutex) & 0xFF0000) // count should be > 0
        if ( !wait )
            return EBUSY;
        BREAK_MASK()
    }

    __divine_assert( (gtid + 1) < (1 << 16) );

    // try to increment lock counter
    int err = _mutex_adjust_count( mutex, 1 );
    if (err)
        return err;

    // lock the mutex
    (*mutex) &= ~0xFFFF;
    (*mutex) |= (gtid + 1);

    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex) {
    if (mutex == NULL)
        return EINVAL;

    if ((*mutex) & 0xFFFF) {
        // mutex is locked
        if ( (*mutex) & PTHREAD_MUTEX_ERRORCHECK )
             return EBUSY;
        else
             __divine_assert( 0 );
    }
    *mutex &= ~_INITIALIZED_MUTEX;
    return 0;
}

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
    __divine_interrupt_mask();

    if (mutex == NULL)
        return EINVAL;

    if ((*mutex) & _INITIALIZED_MUTEX) {
        // already initialized
        if ( (*mutex) & PTHREAD_MUTEX_ERRORCHECK )
            return EBUSY;
    }

    if (attr)
        *mutex = *attr;
    else
        *mutex = PTHREAD_MUTEX_DEFAULT;
    *mutex |= _INITIALIZED_MUTEX;
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex) {
    __divine_interrupt_mask();
    return _mutex_lock( mutex, 1 );
}

int pthread_mutex_trylock(pthread_mutex_t *mutex) {
    __divine_interrupt_mask();
    return _mutex_lock( mutex, 0 );
}

int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    __divine_interrupt_mask();
    int gtid = _get_gtid(__divine_get_tid());
    DBG_ASSERT(gtid >= 0 && gtid < thread_counter)

    if ( mutex == NULL || !((*mutex) & _INITIALIZED_MUTEX) ) {
        return EINVAL; // mutex does not refer to an initialized mutex object
    }

    if (((*mutex) & 0xFFFF) != (gtid + 1)) {
        // mutex is not locked or it is already locked by another thread
        DBG_ASSERT((*mutex) & 0xFF0000) // count should be > 0
        if ( (*mutex) & PTHREAD_MUTEX_NORMAL )
             __divine_assert( 0 );
        else
             return EPERM; // recursive mutex can also detect
    }

    _mutex_adjust_count( mutex, -1);
    if ( !((*mutex) & 0xFF0000) )
        *mutex &= ~0xFFFF; // unlock if count == 0
    return 0;
}

/* Mutex attributes */
int pthread_mutexattr_destroy(pthread_mutexattr_t *attr) {
    if (attr == NULL)
        return EINVAL;

    *attr = 0;
    return 0;
}

int pthread_mutexattr_gettype(const pthread_mutexattr_t *attr, int *value) {
    if (attr == NULL)
        return EINVAL;

    *value = *attr;
    return 0;
}

int pthread_mutexattr_init(pthread_mutexattr_t *attr) {
    if (attr == NULL)
        return EINVAL;

    *attr = PTHREAD_MUTEX_DEFAULT;
    return 0;
}

int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int value) {
    if (attr == NULL)
        return EINVAL;

    *attr = value;
    return 0;
}

/* Thread specific data */
int pthread_key_create(pthread_key_t *p_key, void (*destructor)(void *)) {
    __divine_interrupt_mask();

    // malloc
    void* _key = __divine_malloc( sizeof(_PerThreadData) );
    pthread_key_t key = static_cast< pthread_key_t >(_key);
    if (alloc_pslots) {
        key->data = static_cast< void ** >
                        ( __divine_malloc( sizeof(void*) * alloc_pslots ) );
    } else key->data = NULL;

    // initialize
    key->destructor = destructor;
    for ( int i = 0; i < alloc_pslots; ++i ) {
        key->data[i] = NULL;
    }

    // insert into double-linked list
    key->next = keys;
    if (key->next)
        key->next->prev = key;
    key->prev = NULL;
    keys = key;

    // return
    *p_key = key;
    return 0;
}

int pthread_key_delete(pthread_key_t key) {
    __divine_interrupt_mask();

    if (key == NULL)
        return EINVAL;

    // remove from double-linked list and free
    if (keys == key) {
        keys = key->next;
    }
    if (key->next) {
        key->next->prev = key->prev;
    }
    if (key->prev) {
        key->prev->next = key->next;
    }

    __divine_free( key->data );
    __divine_free( key );
    return 0;
}

int pthread_setspecific(pthread_key_t key, const void *data) {
    __divine_interrupt_mask();
    _initialize();

    if (key == NULL)
        return EINVAL;

    int ltid = __divine_get_tid();
    DBG_ASSERT(ltid < alloc_pslots)

    key->data[ltid] = const_cast<void*>(data);
    return 0;
}

void *pthread_getspecific(pthread_key_t key) {
    __divine_interrupt_mask();
    _initialize();

    __divine_assert(key != NULL);

    int ltid = __divine_get_tid();
    DBG_ASSERT(ltid < alloc_pslots)

    return key->data[ltid];
}

/* Conditional variables */

/*
  pthread_cond_t representation:

    { .mutex: address of associated mutex
      .counter:
          -------------------------------------------------------------------
         | *free* | initialized?: 1 bit | number of waiting threads: 16 bits |
          -------------------------------------------------------------------
      }
*/

int _cond_adjust_count( pthread_cond_t *cond, int adj ) {
    int count = cond->counter & 0xFFFF;
    count += adj;
    __divine_assert(count < (1 << 16));

    (cond->counter) &= ~0xFFFF;
    (cond->counter) |= count;
    return count;
}

int pthread_cond_destroy(pthread_cond_t *cond) {
    if ( cond == NULL || !(cond->counter & _INITIALIZED_COND) )
        return EINVAL;

    // make sure that no thread is waiting on this condition
    // (probably better alternative when compared to: return EBUSY)
    __divine_assert((cond->counter & 0xFFFF) == 0);

    cond->mutex = NULL;
    cond->counter = 0;
    return 0;
}

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t * /* TODO: cond. attributes */) {
    if (cond == NULL)
        return EINVAL;

    if (cond->counter & _INITIALIZED_COND)
        return EBUSY; // already initialized

    cond->mutex = NULL;
    cond->counter = _INITIALIZED_COND;
    return 0;
}

int _cond_signal(pthread_cond_t *cond, bool broadcast = false) {
    if ( cond == NULL || !(cond->counter & _INITIALIZED_COND) )
        return EINVAL;

    int count = cond->counter & 0xFFFF;
    if (count)  { // some threads are waiting for condition
        int waiting = 0, wokenup = 0, choice;

        if (!broadcast) {
            /* non-determinism */
            choice = __divine_choice( (1 << count) - 1 );
        }

        for ( int i = 0; i < alloc_pslots; ++i ) {
            if (!threads[i]) // empty slot
                continue;

            if (threads[i]->sleeping && threads[i]->condition == cond) {
                ++waiting;
                if ( ( broadcast ) ||
                     ( (choice + 1) & (1 << (waiting - 1)) ) ) {
                   // wake up the thread
                   threads[i]->sleeping = false;
                   threads[i]->condition = NULL;
                   ++wokenup;
                }
            }
        }

        DBG_ASSERT( count == waiting )

        if (!_cond_adjust_count(cond,-wokenup))
            cond->mutex = NULL; // break binding between cond. variable and mutex
    }
    return 0;
}

int pthread_cond_signal(pthread_cond_t *cond) {
    __divine_interrupt_mask();
    return _cond_signal(cond);
}

int pthread_cond_broadcast(pthread_cond_t *cond) {
    __divine_interrupt_mask();
    return _cond_signal(cond, true);
}

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
    __divine_interrupt_mask();
    _initialize();

    int ltid = __divine_get_tid();
    DBG_ASSERT(ltid < alloc_pslots)
    int gtid = _get_gtid(__divine_get_tid());
    DBG_ASSERT(gtid >= 0 && gtid < thread_counter)

    if ( cond == NULL || !(cond->counter & _INITIALIZED_COND) )
        return EINVAL;

    if ( mutex == NULL || !((*mutex) & _INITIALIZED_MUTEX) ) {
        return EINVAL;
    }

    if (((*mutex) & 0xFFFF) != (gtid + 1)) {
        // mutex is not locked or it is already locked by another thread
        return EPERM;
    }

    // It is allowed to have one mutex associated with more than one conditional
    // variable. On the other hand, using more than one mutex for one
    // conditional variable results in undefined behaviour.
    __divine_assert(cond->mutex == NULL || cond->mutex == mutex);
    cond->mutex = mutex;

    // fall asleep
    Thread* thread = threads[ltid];
    thread->sleeping = true;
    thread->condition = cond;

    _cond_adjust_count(cond, 1); // one more thread is waiting for this condition
    pthread_mutex_unlock(mutex); // unlock associated mutex

    // sleeping
    while (thread->sleeping)
        BREAK_MASK()

    // try to lock mutex which was associated to the cond. variable by this thread
    // (not the one from current binding, which may have changed)
    pthread_mutex_lock(mutex);
    return 0;
}

/* Once-only execution */
int pthread_once(pthread_once_t *once_control, void (*init_routine)(void)) {
    __divine_interrupt_mask();
    if (*once_control) {
        *once_control = 0;
        __divine_interrupt_unmask();
        __divine_interrupt();
        init_routine();
        __divine_interrupt_mask();
    }
    return 0;
}
