/*
 * OO base class for process functions and child processes
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
#include <wibble/sys/childprocess.h>

#ifdef POSIX

#include <stdlib.h>		// EXIT_FAILURE
#include <sys/types.h>		// fork, waitpid, kill, open, getpw*, getgr*, initgroups
#include <sys/stat.h>		// open
#include <sys/resource.h>	// getrlimit, setrlimit
#include <unistd.h>			// fork, dup2, pipe, close, setsid, _exit, chdir
#include <fcntl.h>			// open
#include <sys/wait.h>		// waitpid
#include <signal.h>			// kill
#include <stdio.h>			// flockfile, funlockfile
#include <ctype.h>			// is*
#include <pwd.h>			// getpw*
#include <grp.h>			// getgr*, initgroups
#include <errno.h>

#include <sstream>

namespace wibble {
namespace sys {

using namespace std;

pid_t ChildProcess::fork()
{
	flockfile(stdin);
	flockfile(stdout);
	flockfile(stderr);

	pid_t pid;
	if ((pid = ::fork()) == 0)
	{
		// Tell the logging system we're in a new process
		//Log::Logger::instance()->setupForkedChild();

		// Child process
		try {
			// no need to funlockfile here, since the library resets the stream
			// locks in the child after a fork

			// Call the process main function and return the exit status
			_exit(main());
		} catch (std::exception& e) {
			//log_err(string(e.type()) + ": " + e.desc());
		}
		_exit(EXIT_FAILURE);
	} else if (pid < 0) {
		funlockfile(stdin);
		funlockfile(stderr);
		funlockfile(stdout);
		throw wibble::exception::System("trying to fork the child process to run action script");
	} else {
		funlockfile(stdin);
		funlockfile(stderr);
		funlockfile(stdout);

		// Parent process
		return _pid = pid;
	}
}

pid_t ChildProcess::forkAndRedirect(int* stdinfd, int* stdoutfd, int* stderrfd)
{
	int pipes[3][2];

	if (stdinfd)
	{
		if (pipe(pipes[0]) == -1)
			throw wibble::exception::System("trying to create the pipe to connect to child standard input");
		*stdinfd = pipes[0][1];
	}
	if (stdoutfd)
	{
		if (pipe(pipes[1]) == -1)
			throw wibble::exception::System("trying to create the pipe to connect to child standard output");
		*stdoutfd = pipes[1][0];
		if (stderrfd == stdoutfd)
			*stderrfd = pipes[1][0];
	}
	
	if (stderrfd && stderrfd != stdoutfd)
	{
		if (pipe(pipes[2]) == -1)
			throw wibble::exception::System("trying to create the pipe to connect to child standard error");
		*stderrfd = pipes[2][0];
	}

	flockfile(stdin);
	flockfile(stdout);
	flockfile(stderr);

	pid_t pid;
	if ((pid = ::fork()) == 0)
	{
		// Tell the logging system we're in a new process
		//Log::Logger::instance()->setupForkedChild();

		// Child process
		try {
			if (stdinfd)
			{
				// Redirect input from the parent to stdin
				if (close(pipes[0][1]) == -1)
					throw wibble::exception::System("closing write end of parent stdin pipe");
				if (dup2(pipes[0][0], 0) == -1)
					throw wibble::exception::System("dup2-ing parent stdin pipe to stdin");
				if (close(pipes[0][0]) == -1)
					throw wibble::exception::System("closing original read end of parent stdin pipe");
			}

			if (stdoutfd)
			{
				// Redirect output to the parent stdout fd
				if (close(pipes[1][0]) == -1)
					throw wibble::exception::System("closing read end of parent stdout pipe");
				if (dup2(pipes[1][1], 1) == -1)
					throw wibble::exception::System("dup2-ing stdout to parent stdout pipe");
				if (stderrfd == stdoutfd)
					if (dup2(pipes[1][1], 2) == -1)
						throw wibble::exception::System("dup2-ing stderr to parent stdout/stderr pipe");
				if (close(pipes[1][1]) == -1)
					throw wibble::exception::System("closing original write end of parent stdout pipe");
			}

			if (stderrfd && stderrfd != stdoutfd)
			{
				// Redirect all output to the parent
				if (close(pipes[2][0]) == -1)
					throw wibble::exception::System("closing read end of parent stderr pipe");
				if (dup2(pipes[2][1], 2) == -1)
					throw wibble::exception::System("dup2-ing stderr to parent stderr pipe");
				if (close(pipes[2][1]) == -1)
					throw wibble::exception::System("closing original write end of parent stderr pipe");
			}

			// Call the process main function and return the exit status
			_exit(main());
		} catch (std::exception& e) {
			//log_err(string(e.type()) + ": " + e.desc());
		}
		_exit(EXIT_FAILURE);
	} else if (pid < 0) {
		funlockfile(stdin);
		funlockfile(stderr);
		funlockfile(stdout);
		if (stdinfd)
		{
			close(pipes[0][0]);
			close(pipes[0][1]);
		}
		if (stdoutfd)
		{
			close(pipes[1][0]);
			close(pipes[1][1]);
		}
		if (stderrfd && stderrfd != stdoutfd)
		{
			close(pipes[2][0]);
			close(pipes[2][1]);
		}
		throw wibble::exception::System("trying to fork the child process to run action script");
	} else {
		funlockfile(stdin);
		funlockfile(stderr);
		funlockfile(stdout);

		// Parent process
		_pid = pid;
		try {
			if (stdinfd)
				if (close(pipes[0][0]) == -1)
					throw wibble::exception::System("closing read end of stdin child pipe");
			if (stdoutfd)
				if (close(pipes[1][1]) == -1)
					throw wibble::exception::System("closing write end of stdout child pipe");
			if (stderrfd && stderrfd != stdoutfd)
				if (close(pipes[2][1]) == -1)
					throw wibble::exception::System("closing write end of stderr child pipe");
			return pid;
		} catch (wibble::exception::System& e) {
			// Try to kill the child process if any errors occurs here
			::kill(pid, 15);
			throw e;
		}
	}
}

void ChildProcess::waitError() {
    if (errno == EINTR)
        throw wibble::exception::Interrupted("waiting for child termination");
    else
        throw wibble::exception::System("waiting for child termination");
}

int ChildProcess::wait()
{
	if (_pid == -1)
	{
		//log_debug("Child already finished");
		return -1;		// FIXME: for lack of better ideas
	}

	if (waitpid(_pid, &m_status, 0) == -1)
            waitError();
	_pid = -1;
	return m_status;
}

bool ChildProcess::running()
{
    if ( _pid == -1 ) {
        return false;
    }

    int res = waitpid(_pid, &m_status, WNOHANG);
        
    if ( res == -1 ) {
        waitError();
    }

    if ( !res ) {
        return true;
    }

    return false;
}

int ChildProcess::wait(struct rusage* ru)
{
	if (_pid == -1)
	{
		//log_debug("Child already finished");
		return -1;		// FIXME: for lack of better ideas
	}

	if (wait4(_pid, &m_status, 0, ru) == -1)
            waitError();

	_pid = -1;
	return m_status;
}

void ChildProcess::waitForSuccess() {
    int r = wait();
    if ( WIFEXITED( r ) ) {
        if ( WEXITSTATUS( r ) )
            throw exception::Generic(
                str::fmtf( "Subprocess terminated with error %d.",
                           WEXITSTATUS( r ) ) );
        else
            return;
    }
    if ( WIFSIGNALED( r ) )
        throw exception::Generic(
            str::fmtf( "Subprocess terminated by signal %d.",
                       WTERMSIG( r ) ) );
    throw exception::Generic( "Error waiting for subprocess." );
}

void ChildProcess::kill(int signal)
{
	if (::kill(_pid, signal) == -1)
	{
		stringstream str;
		str << "killing process " + _pid;
		throw wibble::exception::System(str.str());
	}
}

}
}
#endif
// vim:set ts=4 sw=4:
