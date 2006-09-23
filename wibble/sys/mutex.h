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
#include <pthread.h>

namespace wibble {
namespace sys {

/**
 * pthread mutex wrapper
 */
class Mutex
{
protected:
	pthread_mutex_t mutex;
	
public:
	Mutex()
	{
		if (int res = pthread_mutex_init(&mutex, 0))
			throw wibble::exception::System(res, "creating pthread mutex");
	}
	~Mutex()
	{
		if (int res = pthread_mutex_destroy(&mutex))
			throw wibble::exception::System(res, "destroying pthread mutex");
	}

	/// Lock the mutex
	/// Normally it's better to use MutexLock
	void lock()
	{
		if (int res = pthread_mutex_lock(&mutex))
			throw wibble::exception::System(res, "locking pthread mutex");
	}

	/// Unlock the mutex
	/// Normally it's better to use MutexLock
	void unlock()
	{
		if (int res = pthread_mutex_unlock(&mutex))
			throw wibble::exception::System(res, "unlocking pthread mutex");
	}

	/// Reinitialize the mutex
	void reinit()
	{
		if (int res = pthread_mutex_init(&mutex, 0))
			throw wibble::exception::System(res, "reinitialising pthread mutex");
	}

	friend class Condition;
};

/**
 * Acquire a mutex lock, RAII-style
 */
class MutexLock
{
protected:
	Mutex& mutex;
	
public:
	MutexLock(Mutex& m) : mutex(m) { mutex.lock(); }
	~MutexLock() { mutex.unlock(); }

	friend class Condition;
};

/*
 * pthread condition wrapper.
 *
 * It works in association with a MutexLock.
 */
class Condition
{
protected:
	pthread_cond_t cond;

public:
	Condition()
	{
		if (int res = pthread_cond_init(&cond, 0))
			throw wibble::exception::System(res, "creating pthread condition");
	}
	~Condition()
	{
		if (int res = pthread_cond_destroy(&cond))
			throw wibble::exception::System(res, "destroying pthread condition");
	}

	/// Wake up one process waiting on the condition
	void signal()
	{
		if (int res = pthread_cond_signal(&cond))
			throw wibble::exception::System(res, "signaling on a pthread condition");
	}

	/// Wake up all processes waiting on the condition
	void broadcast()
	{
		if (int res = pthread_cond_broadcast(&cond))
			throw wibble::exception::System(res, "broadcasting on a pthread condition");
	}

	/// Wait on the condition, locking with l [for limited time abstime]
	/// (Unlock l before waiting and lock it before returning)
	void wait(MutexLock& l)
	{
		if (int res = pthread_cond_wait(&cond, &l.mutex.mutex))
			throw wibble::exception::System(res, "waiting on a pthread condition");
	}
	void wait(MutexLock& l, const struct timespec& abstime)
	{
		if (int res = pthread_cond_timedwait(&cond, &l.mutex.mutex, &abstime))
			throw wibble::exception::System(res, "waiting on a pthread condition");
	}
};

}
}

// vim:set ts=4 sw=4:
#endif
