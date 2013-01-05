// -*- C++ -*-
#ifndef WIBBLE_SYS_SIGNAL_H
#define WIBBLE_SYS_SIGNAL_H

#include <wibble/sys/macros.h>
#include <wibble/exception.h>
#include <signal.h>

namespace wibble {
namespace sys {
namespace sig {

#ifdef POSIX

/**
 * RAII-style sigprocmask wrapper
 */
struct ProcMask
{
	sigset_t oldset;
	ProcMask(const sigset_t& newset, int how = SIG_BLOCK)
	{
		if (sigprocmask(how, &newset, &oldset) < 0)
			throw wibble::exception::System("setting signal mask");
	}

	~ProcMask()
	{
		if (sigprocmask(SIG_SETMASK, &oldset, NULL) < 0)
			throw wibble::exception::System("restoring signal mask");
	}
};

struct Action
{
	int signum;
	struct sigaction oldact;

	Action(int signum, const struct sigaction& act) : signum(signum)
	{
		if (sigaction(signum, &act, &oldact) < 0)
			throw wibble::exception::System("setting signal action");
	}
	~Action()
	{
		if (sigaction(signum, &oldact, NULL) < 0)
			throw wibble::exception::System("restoring signal action");
	}
};

#endif

}
}
}

// vim:set ts=4 sw=4:
#endif
