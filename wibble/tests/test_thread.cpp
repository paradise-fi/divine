#include <wibble/config.h>
#include <wibble/sys/mutex.h>
#include <wibble/sys/thread.h>
using namespace std;
using namespace wibble::sys;

#include <wibble/tests/tut-wibble.h>

namespace tut {

struct sys_thread_shar {};
TESTGRP( sys_thread );

// Test threads that just assigns a value to an int and exists
class TestThread1 : public Thread
{
protected:
	int& res;
	int val;

	void* main()
	{
		res = val;
		return (void*)val;
	}
public:
	TestThread1(int& res, int val) : res(res), val(val) {}
};

// Thread that continuously increments an int value
class TestThread2 : public Thread
{
protected:
	int& res;
	Mutex& mutex;
	bool done;

	void* main()
	{
		while (!done)
		{
			MutexLock lock(mutex);
			++res;
		}
		return 0;
	}

public:
	TestThread2(int& res, Mutex& mutex) : res(res), mutex(mutex), done(false) {}
	void quit() { done = true; }
};

// Test that threads are executed
template<> template<>
void to::test< 1 >() {
	int val = 0;

	TestThread1 assigner(val, 42);
	assigner.start();
	ensure_equals(assigner.join(), (void*)42);
	ensure_equals(val, 42);
}

// Use mutexes to access shared memory
template<> template<>
void to::test< 2 >() {
	int val = 0;
	Mutex mutex;

	TestThread2 incrementer(val, mutex);
	incrementer.start();

	bool done = false;
	while (!done)
	{
		MutexLock lock(mutex);
		if (val > 100)
			done = true;
	}
	incrementer.quit();
	ensure_equals(incrementer.join(), (void*)0);
}


}

// vim:set ts=4 sw=4:
