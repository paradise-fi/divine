#define _THREAD_PRIVATE_MUTEX_LOCK(name)		do {} while (0)
#define _THREAD_PRIVATE_MUTEX_UNLOCK(name)		do {} while (0)
#define _THREAD_PRIVATE(keyname, storage, error)	&(storage)
#define _MUTEX_LOCK(mutex)				do {} while (0)
#define _MUTEX_UNLOCK(mutex)				do {} while (0)
#define _MUTEX_DESTROY(mutex)				do {} while (0)
#define _MALLOC_LOCK(n)					do {} while (0)
#define _MALLOC_UNLOCK(n)				do {} while (0)
#define _ATEXIT_LOCK()					do {} while (0)
#define _ATEXIT_UNLOCK()				do {} while (0)
#define _ATFORK_LOCK()					do {} while (0)
#define _ATFORK_UNLOCK()				do {} while (0)
#define _ARC4_LOCK()					do {} while (0)
#define _ARC4_UNLOCK()					do {} while (0)

/*
 * helper macro to make unique names in the thread namespace
 */
#define __THREAD_NAME(name) _thread_tagname_ ## name

/*
 * Macros used in libc to access thread mutex, keys, and per thread storage.
 * _THREAD_PRIVATE_KEY and _THREAD_PRIVATE_MUTEX are different macros for
 * historical reasons.   They do the same thing, define a static variable
 * keyed by 'name' that identifies a mutex and a key to identify per thread
 * data.
 */
#define _THREAD_PRIVATE_KEY(name)					\
	static void *__THREAD_NAME(name)
#define _THREAD_PRIVATE_MUTEX(name)					\
	static void *__THREAD_NAME(name)
