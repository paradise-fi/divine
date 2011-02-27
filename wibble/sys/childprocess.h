// -*- C++ -*-

#ifndef WIBBLE_SYS_CHILDPROCESS_H
#define WIBBLE_SYS_CHILDPROCESS_H

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

#include <sys/types.h>
#include <wibble/exception.h>
#include <wibble/sys/macros.h>

#ifdef _WIN32
#include <windows.h>
#endif

struct rusage;

namespace wibble {
namespace sys {

/**
 * Fork a child process
 */
class ChildProcess
{
protected:
    pid_t _pid;
    int pipes[3][2];

    int *_stdin, *_stdout, *_stderr;
    int m_status;
    bool m_doExec;
    std::string m_command;

#ifdef _WIN32
    int backups[3];
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
#endif

    /**
     * Main function to be called in the child process after it has forked
     */
    // TODO: since the destructor is called twice (one in the parent and one in
    // the child), it could be useful to add a bool isChild() method to let the
    // destructor and other functions know where they are operating.  The value
    // returned can be set by ChildProcess::fork.

    virtual int main() = 0;

    /**
     * On Windows, it's impossible to fork(), but if you were to fork+exec,
     * it's not all lost. You can implement spawnChild() instead of main(),
     * which needs to call CreateProcess, spawn or similar. The redirections
     * requested by setupRedirects are respected. Exec and ShellProcess
     * implement spawnChild on Windows.
     *
     * NB. For wait() to work, the si/pi member variables need to be filled in
     * by the implementation.
     */
    virtual void spawnChild();

    void waitError();
    void setupPipes();
    void setupPrefork();
    void setupChild();
    void setupParent();

public:
    ChildProcess() : _pid(-1), _stdin( 0 ), _stdout( 0 ), _stderr( 0 ) {}
    virtual ~ChildProcess() {}

    /// Instead of calling the main() function of this class, execute an
    /// external command. The command is passed to the shell interpreter of the
    /// system (/bin/sh on UNIX, CreateProcess on Windows).
    void setExec( std::string command ) {
        m_doExec = true;
        m_command = command;
    }

    /// For a subprocess to run proc. To redirect stdio of the child process to
    /// pipes, call setupRedirects first. NB. This currently works on Windows
    /// only when setExec has been called first (any main() overrides have no
    /// effect on Windows).
    pid_t fork();

    void setupRedirects(int* stdinfd = 0, int* stdoutfd = 0, int* stderrfd = 0);

    pid_t forkAndRedirect(int* stdinfd = 0, int* stdoutfd = 0, int* stderrfd = 0) {
        setupRedirects(stdinfd, stdoutfd, stderrfd);
        return fork();
    }

    /**
     * Get the pid of the child process or (pid_t)-1 if no child is running
     *
     * Note: while ChildProcess::kill() has a safeguard against killing pid -1,
     * if you are going to run ::kill on the output of pid() make sure to check
     * what is the semanthics of kill() when pid is -1.
     */
    pid_t pid() const { return _pid; }

    bool running();
    int exitStatus();
    void waitForSuccess();

    /// Wait for the child to finish, returning its exit status and optionally
    /// storing resource usage informations in `ru'.  Return -1 if no child is
    /// running.  TODO: gracefully handle the EINTR error code
    int wait(struct rusage* ru = 0);

    /// Send the given signal to the process
    void kill(int signal);
};

}
}

// vim:set ts=4 sw=4:
#endif
