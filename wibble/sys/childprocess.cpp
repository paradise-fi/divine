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

#include <stdlib.h>		// EXIT_FAILURE
#include <sys/types.h>		// fork, waitpid, kill, open, getpw*, getgr*, initgroups
#include <sys/stat.h>		// open
#include <unistd.h>			// fork, dup2, pipe, close, setsid, _exit, chdir
#include <fcntl.h>			// open

#ifdef POSIX
#include <sys/resource.h>	// getrlimit, setrlimit
#include <sys/wait.h>		// waitpid
#include <signal.h>			// kill
#include <pwd.h>			// getpw*
#include <grp.h>			// getgr*, initgroups
#endif

#include <stdio.h>			// flockfile, funlockfile
#include <ctype.h>			// is*
#include <errno.h>

#include <sstream>

namespace wibble {
namespace sys {

using namespace std;
namespace wexcept = wibble::exception;

void ChildProcess::spawnChild() {
    assert_die();
}

#ifndef POSIX
void funlockfile( FILE * ) {}
void flockfile( FILE * ) {}
#endif

#ifdef POSIX
pid_t ChildProcess::fork()
{
    flockfile(stdin);
    flockfile(stdout);
    flockfile(stderr);

    setupPrefork();

    pid_t pid;
    if ((pid = ::fork()) == 0)
    {
        // Tell the logging system we're in a new process
        //Log::Logger::instance()->setupForkedChild();

        // Child process
        try {
            setupChild();

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
        throw wexcept::System("trying to fork a child process");

    } else {
        funlockfile(stdin);
        funlockfile(stderr);
        funlockfile(stdout);

        _pid = pid;
        setupParent();

        // Parent process
        return _pid;
    }
}
#elif defined _WIN32

pid_t ChildProcess::fork() {
    setupPrefork();

    /* Copy&paste from MSDN... I don't want to know. */
    ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);
    ZeroMemory( &pi, sizeof(pi) );
    /* End of MSDN. */

    spawnChild();
    setupParent();

    return 1; // FIXME is there anything like a pid on win32?
}
#endif

void mkpipe( int *fds, int *infd, int *outfd, const char *err )
{
#ifdef POSIX
    if (pipe(fds) == -1)
#elif defined _WIN32
    if (_pipe(fds, 1000, _O_BINARY) == -1)
#endif
        throw wexcept::System( err );
    if (infd)
        *infd = fds[0];
    if (outfd)
        *outfd = fds[1];
}

void renamefd( int _old, int _new, const char *err = "..." )
{
    if ( dup2( _old, _new ) == -1 )
        throw wexcept::System( err );
    if ( close( _old ) == -1 )
        throw wexcept::System( err );
}

void ChildProcess::setupRedirects(int* _stdinfd, int* _stdoutfd, int* _stderrfd) {
    _stdin = _stdinfd;
    _stdout = _stdoutfd;
    _stderr = _stderrfd;

    if (_stdin)
        mkpipe( pipes[0], 0, _stdin,
                "trying to create the pipe to connect to child standard input" );

    if (_stdout)
        mkpipe( pipes[1], _stdout, 0,
                "trying to create the pipe to connect to child standard output" );

    if (_stderr && _stderr != _stdout)
        mkpipe( pipes[2], _stderr, 0,
                "trying to create the pipe to connect to child standard output" );

}

void ChildProcess::setupPrefork()
{
#ifdef _WIN32
    if (_stdin) {
        backups[0] = dup( STDIN_FILENO );
        SetHandleInformation( (HANDLE)_get_osfhandle( pipes[0][1] ), HANDLE_FLAG_INHERIT, 0 );
        dup2( pipes[0][0], STDIN_FILENO );
    }

    if (_stdout) {
        backups[1] = dup( STDOUT_FILENO );
        SetHandleInformation( (HANDLE)_get_osfhandle( pipes[1][0] ), HANDLE_FLAG_INHERIT, 0 );
        dup2( pipes[1][1], STDOUT_FILENO );
    }

    if ( _stderr ) {
        backups[2] = dup( STDERR_FILENO );
        SetHandleInformation( (HANDLE)_get_osfhandle( pipes[2][0] ), HANDLE_FLAG_INHERIT, 0 );
        dup2( pipes[2][1], STDERR_FILENO );
    }
#endif
}

void ChildProcess::setupChild() {
    if (_stdin) {
        // Redirect input from the parent to _stdin
        if (close(pipes[0][1]) == -1)
            throw wexcept::System("closing write end of parent _stdin pipe");

        renamefd( pipes[0][0], STDIN_FILENO, "renaming parent _stdin pipe fd" );
    }

    if (_stdout) {
        // Redirect output to the parent _stdout fd
        if (close(pipes[1][0]) == -1)
            throw wexcept::System("closing read end of parent _stdout pipe");

        if (_stderr == _stdout)
            if (dup2(pipes[1][1], STDERR_FILENO) == -1)
                throw wexcept::System( "dup2-ing _stderr to parent _stdout/_stderr pipe");
        renamefd( pipes[1][1], STDOUT_FILENO, "renaming parent _stdout pipe" );
    }

    if (_stderr && _stderr != _stdout) {
        // Redirect all output to the parent
        if (close(pipes[2][0]) == -1)
            throw wexcept::System("closing read end of parent _stderr pipe");

        renamefd( pipes[2][1], STDERR_FILENO, "renaming parent _stderr pipe" );
    }
}

void ChildProcess::setupParent() {
    funlockfile(stdin);
    funlockfile(stderr);
    funlockfile(stdout);

    if (_stdin && close(pipes[0][0]) == -1)
        throw wexcept::System("closing read end of _stdin child pipe");
    if (_stdout && close(pipes[1][1]) == -1)
        throw wexcept::System("closing write end of _stdout child pipe");
    if (_stderr && _stderr != _stdout && close(pipes[2][1]) == -1)
        throw wexcept::System("closing write end of _stderr child pipe");

#ifdef _WIN32
    if (_stdin)
        dup2( backups[0], STDIN_FILENO );

    if (_stdout)
        dup2( backups[1], STDOUT_FILENO );

    if ( _stderr )
        dup2( backups[2], STDERR_FILENO );
#endif
}

void ChildProcess::waitError() {
    if (errno == EINTR)
        throw wexcept::Interrupted("waiting for child termination");
    else
        throw wexcept::System("waiting for child termination");
}

bool ChildProcess::running()
{
#ifdef POSIX
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
#else
    assert_die();
#endif
}

int ChildProcess::wait(struct rusage* ru)
{
#ifdef POSIX
    if (_pid == -1)
        return -1; // FIXME: for lack of better ideas

    if (wait4(_pid, &m_status, 0, ru) == -1)
        waitError();
#else
    m_status = 0; // FIXME
    WaitForSingleObject( pi.hProcess, INFINITE );
#endif

    _pid = -1;
    return m_status;
}

void ChildProcess::waitForSuccess() {
    int r = wait();
#ifdef POSIX
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"

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
#pragma GCC diagnostic pop
#endif
}

void ChildProcess::kill(int signal)
{
#ifdef POSIX
    if (_pid == -1)
        throw wexcept::Consistency(
            "killing child process",
            "child process has not been started");
    if (::kill(_pid, signal) == -1)
    {
        stringstream str;
        str << "killing process " << _pid;
        throw wibble::exception::System(str.str());
    }
#else
    assert_die();
#endif
}

}
}
// vim:set ts=4 sw=4:
