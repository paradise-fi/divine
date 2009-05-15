/*
 * OO encapsulation of Posix threads
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

#include <wibble/sys/thread.h>
#include <sstream>

//using namespace std;

namespace wibble {
namespace sys {

#ifdef POSIX
void* Thread::Starter(void* parm)
{
	return ((Thread*)parm)->main();
}
#endif

#ifdef _WIN32
unsigned __stdcall Thread::Starter(void* parm)
{
	void* vptemp = ((Thread*)parm)->main();
	return (unsigned)vptemp;
}
#endif

void Thread::testcancel()
{
#ifdef POSIX
	pthread_testcancel();
#endif
}

void Thread::start()
{
  int res = 1;
#ifdef POSIX
  res = pthread_create(&thread, 0, Starter, this);
#endif

#ifdef _WIN32
  hThread = (HANDLE)_beginthreadex(NULL, 0, &Starter, this, 0, &thread);
  if(hThread != NULL)
    res = 0;
#endif

	if (res != 0)
		throw wibble::exception::System(res, std::string("Creating ") + threadTag() + " thread");
}

void Thread::startDetached()
{
#ifdef POSIX
	pthread_attr_t thread_attrs;
	pthread_attr_init(&thread_attrs);
	pthread_attr_setdetachstate(&thread_attrs, PTHREAD_CREATE_DETACHED);
	int res = pthread_create(&thread, &thread_attrs, Starter, this);
	pthread_attr_destroy(&thread_attrs);
	if (res != 0)
		throw wibble::exception::System(res, std::string("Creating ") + threadTag() + " thread");
#endif
}

void* Thread::join()
{
	void* result = 0;
	int res = 1;

#ifdef POSIX
	res = pthread_join(thread, &result);
#endif

#ifdef _WIN32
  if(GetCurrentThreadId() != thread)
	if(WaitForSingleObject(hThread, INFINITE) == WAIT_OBJECT_0)
	{
	  res = 0;
      CloseHandle(hThread);
	}
#endif
	
	if (res != 0)
		throw wibble::exception::System(res, std::string("Joining ") + threadTag() + " thread");
	return result;
}

void Thread::detach()
{
#ifdef POSIX
	int res = pthread_detach(thread);
	if (res != 0)
		throw wibble::exception::System(res, std::string("Detaching ") + threadTag() + " thread");
#endif
}

void Thread::cancel()
{
#ifdef POSIX
	int res = pthread_cancel(thread);
	if (res != 0)
		throw wibble::exception::System(res, std::string("Cancelling ") + threadTag() + " thread");
#endif
}

void Thread::kill(int signal)
{
#ifdef POSIX
	int res = pthread_kill(thread, signal);
	if (res != 0)
	{
		std::stringstream str;
		str << "Killing " << threadTag() << " thread with signal " << signal;
		throw wibble::exception::System(res, str.str());
	}
#endif
}

}
}

// vim:set ts=4 sw=4:
