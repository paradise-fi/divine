#ifndef WIBBLE_SYS_MUTEX_H
#define WIBBLE_SYS_MUTEX_H

/*
 * Encapsulated pthread mutex and condition
 *
 * Copyright (C) 2003--2006  Enrico Zini <enrico@debian.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 */

#include <wibble/exception.h>
#ifdef POSIX
#include <pthread.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <queue>
#include <time.h>
struct timespec 
{
  time_t   tv_sec;        /* seconds */
  long     tv_nsec;       /* nanoseconds */
};
#endif
#include <errno.h>

namespace wibble {
namespace sys {

/**
 * pthread mutex wrapper
 */
class Mutex
{
protected:
#ifdef POSIX
	pthread_mutex_t mutex;
#endif

#ifdef _WIN32
  CRITICAL_SECTION CriticalSection;
#endif
	
public:
  Mutex(bool recursive = false)
	{
    int res = 1;
#ifdef POSIX
            pthread_mutexattr_t attr;
            pthread_mutexattr_init( &attr );
            if ( recursive ) {
#ifdef __APPLE__
                pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_RECURSIVE );
#else
                pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_RECURSIVE_NP );
#endif
            }
    res = pthread_mutex_init(&mutex, &attr);
#endif

#ifdef _WIN32
    InitializeCriticalSection(&CriticalSection);
    res = 0;
#endif
		if (res != 0)
			throw wibble::exception::System(res, "creating pthread mutex");
	}
#ifdef _WIN32
	void initialize()
	{
		InitializeCriticalSection(&CriticalSection);
	}
#endif
	~Mutex()
	{
		int res = 1;
#ifdef POSIX
		res = pthread_mutex_destroy(&mutex);
#endif

#ifdef _WIN32
      DeleteCriticalSection(&CriticalSection);
      res = 0;
#endif
		if (res != 0)
			throw wibble::exception::System(res, "destroying pthread mutex");
	}

        bool trylock()
        {
    		int res = 1;
#ifdef POSIX
    		res = pthread_mutex_trylock(&mutex);
    		if ( res == EBUSY )
    			return false;
    		if ( res == 0 )
    			return true;
#endif

#ifdef _WIN32
          res = TryEnterCriticalSection(&CriticalSection);
          if(res == 0)
            return false;
          else
            return true;
#endif
    		throw wibble::exception::System(res, "(try)locking pthread mutex");
        }

	/// Lock the mutex
	/// Normally it's better to use MutexLock
	void lock()
	{
		int res = 1;
#ifdef POSIX
		res = pthread_mutex_lock(&mutex);
#endif

#ifdef _WIN32
      EnterCriticalSection(&CriticalSection);
      res = 0;
#endif
		if (res != 0)
			throw wibble::exception::System(res, "locking pthread mutex");
	}

	/// Unlock the mutex
	/// Normally it's better to use MutexLock
	void unlock()
	{
		int res = 1;
#ifdef POSIX
		res = pthread_mutex_unlock(&mutex);
#endif

#ifdef _WIN32
      LeaveCriticalSection(&CriticalSection);
      res = 0;
#endif
		if (res != 0)
			throw wibble::exception::System(res, "unlocking pthread mutex");
	}

	/// Reinitialize the mutex
	void reinit()
	{
#ifdef POSIX
		if (int res = pthread_mutex_init(&mutex, 0))
			throw wibble::exception::System(res, "reinitialising pthread mutex");
#endif
	}

	friend class Condition;
};

/**
 * Acquire a mutex lock, RAII-style
 */
template< typename Mutex >
class MutexLockT
{
public:
	Mutex& mutex;
        bool locked;
        bool yield;

        MutexLockT(Mutex& m) : mutex(m), locked( false ), yield( false ) {
            mutex.lock();
            locked = true;
        }

	~MutexLockT() {
            if ( locked ) {
                mutex.unlock();
                checkYield();
            }
        }

        void drop() {
            mutex.unlock();
            locked = false;
            checkYield();
        }
        void reclaim() { mutex.lock(); locked = true; }
        void setYield( bool y ) {
            yield = y;
        }

        void checkYield() {
#ifdef POSIX
            if ( yield )
                sched_yield();
#endif
        }

	friend class Condition;
};

typedef MutexLockT< Mutex > MutexLock;

/*
 * pthread condition wrapper.
 *
 * It works in association with a MutexLock.
 */
class Condition
{
protected:
#ifdef POSIX
  pthread_cond_t cond;
#endif

#ifdef _WIN32
  std::queue<HANDLE *> qEvent;
  CRITICAL_SECTION CSQueueAccess;
#endif

public:
	Condition()
	{
		int res = 1;
#ifdef POSIX
		res = pthread_cond_init(&cond, 0);
#endif

#ifdef _WIN32
      InitializeCriticalSection(&CSQueueAccess);
      res = 0;
#endif
		if (res != 0)
			throw wibble::exception::System(res, "creating pthread condition");
	}
#ifdef _WIN32
	void initialize()
	{
		InitializeCriticalSection(&CSQueueAccess);
	}
#endif
	~Condition()
	{
		int res = 0;
#ifdef POSIX
		res = pthread_cond_destroy(&cond);
#endif

#ifdef _WIN32
	  EnterCriticalSection(&CSQueueAccess);
      while(!qEvent.empty())
      {
        if(CloseHandle(*(qEvent.front())) == 0)
          res = 1;
        qEvent.pop();   
      }
	  LeaveCriticalSection(&CSQueueAccess);
	  DeleteCriticalSection(&CSQueueAccess);
#endif
		if (res != 0)
			throw wibble::exception::System(res, "destroying pthread condition");
	}

	/// Wake up one process waiting on the condition
	void signal()
	{
		int res = 1;
#ifdef POSIX
		res = pthread_cond_signal(&cond);
#endif

#ifdef _WIN32
      EnterCriticalSection(&CSQueueAccess);
      if(!qEvent.empty() && SetEvent(*(qEvent.front())) != 0)
      {
        res = 0;
        qEvent.pop();
      }
	  else if(qEvent.empty())
	    res = 0;
      LeaveCriticalSection(&CSQueueAccess);
#endif
		if (res != 0)
			throw wibble::exception::System(res, "signaling on a pthread condition");
	}

	/// Wake up all processes waiting on the condition
	void broadcast()
	{
		int res = 0;
#ifdef POSIX
		res = pthread_cond_broadcast(&cond);
#endif

#ifdef _WIN32
      EnterCriticalSection(&CSQueueAccess);
      while(!qEvent.empty())
      {
        if(SetEvent(*(qEvent.front())) != 0)
          qEvent.pop();
        else
          res = 1;
      }
      LeaveCriticalSection(&CSQueueAccess);
#endif
		if (res != 0)
			throw wibble::exception::System(res, "broadcasting on a pthread condition");
	}

	/**
	 * Wait on the condition, locking with l.  l is unlocked before waiting and
	 * locked again before returning.
	 */
    void wait(MutexLock& l)
	{
		int res = 1;
#ifdef POSIX
		res = pthread_cond_wait(&cond, &l.mutex.mutex);
#endif

#ifdef _WIN32
	  EnterCriticalSection(&CSQueueAccess);
      HANDLE thEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
      if(thEvent != NULL)
      {
	    l.mutex.unlock();
        qEvent.push(&thEvent);
        LeaveCriticalSection(&CSQueueAccess);
        if(WaitForSingleObject(thEvent, INFINITE) == WAIT_OBJECT_0 && CloseHandle(thEvent) != 0)
        {
          res = 0;
          l.mutex.lock();
        }
      }
	  else
		LeaveCriticalSection(&CSQueueAccess);
#endif
		if (res != 0)
			throw wibble::exception::System(res, "waiting on a pthread condition");
	}

	void wait(Mutex& l)
	{
		int res = 1;
#ifdef POSIX
		res = pthread_cond_wait(&cond, &l.mutex);
#endif

#ifdef _WIN32
	  EnterCriticalSection(&CSQueueAccess);
      HANDLE thEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
      if(thEvent != NULL)
      {
	    l.unlock();
        qEvent.push(&thEvent);
        LeaveCriticalSection(&CSQueueAccess);
        if(WaitForSingleObject(thEvent, INFINITE) == WAIT_OBJECT_0 && CloseHandle(thEvent) != 0)
        {
          res = 0;
          l.lock();
        }
      }
	  else
		LeaveCriticalSection(&CSQueueAccess);
#endif
		if (res != 0)
			throw wibble::exception::System(res, "waiting on a pthread condition");
	}

	/**
	 * Wait on the condition, locking with l.  l is unlocked before waiting and
	 * locked again before returning.  If the time abstime is reached before
	 * the condition is signaled, then l is locked and the function returns
	 * false.
	 *
	 * @returns
	 *   true if the wait succeeded, or false if the timeout was reached before
	 *   the condition is signaled.
	 */
	bool wait(MutexLock& l, const struct timespec& abstime);
};

}
}

// vim:set ts=4 sw=4:
#endif
