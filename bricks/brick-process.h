// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * Utilities for executing children processes and communication tools between
 * the parent and a child process. Includes:
 * - ChildProcess
 * - Exec
 * - ShellCommand
 * - Pipe
 * - PipeThrough
 */

/*
 * (c) 2008 Petr Ročkai <me@mornfall.net>
 * (c) 2003-2006 Enrico Zini <enrico@debian.org>
 */

/* Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE. */

#include <stdlib.h>     // EXIT_FAILURE
#include <sys/types.h>      // fork, waitpid, kill, open, getpw*, getgr*, initgroups
#include <sys/stat.h>       // open
#include <unistd.h>         // fork, dup2, pipe, close, setsid, _exit, chdir
#include <fcntl.h>          // open

#ifndef _WIN32
#include <sys/resource.h>   // getrlimit, setrlimit
#include <sys/wait.h>       // waitpid
#include <signal.h>         // kill
#include <pwd.h>            // getpw*
#include <grp.h>            // getgr*, initgroups
#endif

#include <stdio.h>          // flockfile, funlockfile
#include <ctype.h>          // is*
#include <errno.h>
#include <string.h>

#include <sstream>
#include <system_error>
#include <stdexcept>
#include <string>
#include <iostream>
#include <mutex>
#include <algorithm> // std::find

#ifdef _WIN32
#include <windows.h>
#include <malloc.h>
#endif

#include <brick-assert.h>
#include <brick-string.h>
#include <brick-shmem.h>
#include <brick-unittest.h>

#ifndef BRICK_PROCESS_H
#define BRICK_PROCESS_H

extern char **environ;

namespace brick {
namespace process {

#ifdef _WIN32
inline void funlockfile( FILE * ) {}
inline void flockfile( FILE * ) {}
#endif

inline void mkpipe( int *fds, int *infd, int *outfd, const char *err ) {
#ifdef _WIN32
    if (_pipe(fds, 1000, _O_BINARY) == -1)
#else
    if (pipe(fds) == -1)
#endif
        throw std::system_error( errno, std::system_category(), err );
    if (infd)
        *infd = fds[0];
    if (outfd)
        *outfd = fds[1];
}

inline void renamefd( int _old, int _new, const char *err = "..." ) {
    if ( dup2( _old, _new ) == -1 )
        throw std::system_error( errno, std::system_category(), err );
    if ( close( _old ) == -1 )
        throw std::system_error( errno, std::system_category(), err );
}


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
    virtual void spawnChild() {
        ASSERT_UNIMPLEMENTED();
    }

    void waitError() {
        if (errno == EINTR)
            throw std::system_error( errno, std::system_category(), "waiting for child termination");
        else
            throw std::system_error( errno, std::system_category(), "waiting for child termination");
    }

    void setupPrefork() {
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

    void setupChild() {
        if (_stdin) {
            // Redirect input from the parent to _stdin
            if (close(pipes[0][1]) == -1)
                throw std::system_error( errno, std::system_category(), "closing write end of parent _stdin pipe");

            renamefd( pipes[0][0], STDIN_FILENO, "renaming parent _stdin pipe fd" );
        }

        if (_stdout) {
            // Redirect output to the parent _stdout fd
            if (close(pipes[1][0]) == -1)
                throw std::system_error( errno, std::system_category(), "closing read end of parent _stdout pipe");

            if (_stderr == _stdout)
                if (dup2(pipes[1][1], STDERR_FILENO) == -1)
                    throw std::system_error( errno, std::system_category(), "dup2-ing _stderr to parent _stdout/_stderr pipe");
            renamefd( pipes[1][1], STDOUT_FILENO, "renaming parent _stdout pipe" );
        }

        if (_stderr && _stderr != _stdout) {
            // Redirect all output to the parent
            if (close(pipes[2][0]) == -1)
                throw std::system_error( errno, std::system_category() , "closing read end of parent _stderr pipe");

            renamefd( pipes[2][1], STDERR_FILENO, "renaming parent _stderr pipe" );
        }
    }

    void setupParent() {
        funlockfile(stdin);
        funlockfile(stderr);
        funlockfile(stdout);

        if (_stdin && close(pipes[0][0]) == -1)
            throw std::system_error( errno, std::system_category(), "closing read end of _stdin child pipe");
        if (_stdout && close(pipes[1][1]) == -1)
            throw std::system_error( errno, std::system_category(), "closing write end of _stdout child pipe");
        if (_stderr && _stderr != _stdout && close(pipes[2][1]) == -1)
            throw std::system_error( errno, std::system_category(), "closing write end of _stderr child pipe");

#ifdef _WIN32
        if (_stdin)
            dup2( backups[0], STDIN_FILENO );

        if (_stdout)
            dup2( backups[1], STDOUT_FILENO );

        if ( _stderr )
            dup2( backups[2], STDERR_FILENO );
#endif
    }

public:
    ChildProcess() : _pid(-1), _stdin( 0 ), _stdout( 0 ), _stderr( 0 ) {}
    virtual ~ChildProcess() {
        wait();
    }

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
    pid_t fork() {
#ifdef _WIN32
        setupPrefork();

        /* Copy&paste from MSDN... I don't want to know. */
        ZeroMemory( &si, sizeof(si) );
        si.cb = sizeof(si);
        ZeroMemory( &pi, sizeof(pi) );
        /* End of MSDN. */

        spawnChild();
        setupParent();

        return 1; // FIXME is there anything like a pid on win32?
#else
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
            throw std::system_error( errno, std::system_category(), "trying to fork a child process" );

        } else {
            funlockfile(stdin);
            funlockfile(stderr);
            funlockfile(stdout);

            _pid = pid;
            setupParent();

            // Parent process
            return _pid;
        }
#endif
    }

    void setupRedirects(int* stdinfd = 0, int* stdoutfd = 0, int* stderrfd = 0) {
        _stdin = stdinfd;
        _stdout = stdoutfd;
        _stderr = stderrfd;

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

    bool running() {
#ifdef _WIN32
        ASSERT_UNIMPLEMENTED();
#else
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
#endif
    }

    void waitForSuccess() {
        int r UNUSED = wait();
#ifndef _WIN32
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"

        if ( WIFEXITED( r ) ) {
            if ( WEXITSTATUS( r ) )
                throw std::runtime_error(
                    brick::string::fmtf( "Subprocess terminated with error %d.",
                               WEXITSTATUS( r ) ) );
            else
                return;
        }
        if ( WIFSIGNALED( r ) )
            throw std::runtime_error(
                brick::string::fmtf( "Subprocess terminated by signal %d.",
                           WTERMSIG( r ) ) );
        throw std::runtime_error( "Error waiting for subprocess." );
#pragma GCC diagnostic pop
#endif
    }

    /// Wait for the child to finish, returning its exit status and optionally
    /// storing resource usage informations in `ru'.  Return -1 if no child is
    /// running.  TODO: gracefully handle the EINTR error code
    int wait(struct rusage* ru = 0) {
        if (_pid == -1)
            return -1; // FIXME: for lack of better ideas
#ifdef _WIN32
        m_status = 0; // FIXME
        WaitForSingleObject( pi.hProcess, INFINITE );
#else
        if (wait4(_pid, &m_status, 0, ru) == -1)
            waitError();
#endif
        _pid = -1;
        return m_status;
    }

    /// Send the given signal to the process
    void kill(int signal) {
#ifdef _WIN32
        ASSERT_UNIMPLEMENTED();
#else
        if (_pid == -1)
            throw std::logic_error(
                std::string( "killing child process: " ) +
                "child process has not been started" );
        if (::kill(_pid, signal) == -1) {
            std::stringstream str;
            str << "killing process " << _pid;
            throw std::system_error( errno, std::system_category(), str.str() );
        }
#endif
    }
};

/**
 * Execute external commands, either forked as a ChildProcess or directly using
 * exec().
 */
class Exec : public ChildProcess
{
protected:
    /**
     * Used to run the program as a child process, if one of the
     * ChildProcess::fork functions is called. Simply calls exec()
     */
    virtual int main() {
        try {
            exec();
        } catch (std::exception& e) {
            std::cerr << e.what() << std::endl;
        }
        return 0;
    }
    virtual void spawnChild() {
#ifdef _WIN32
        std::string cmd = pathname;
        for ( int i = 1; i < int( args.size() ); ++i )
            cmd += " \"" + args[i] + "\""; // FIXME: quoting...
        std::cerr << "CreateProcess: " << cmd << std::endl;
        CreateProcess( NULL, &cmd.front(), NULL, NULL, 1, 0, NULL, NULL, &si, &pi );
#endif
    }

public:
    virtual ~Exec() {}

    /**
     * Filename or pathname of the program to execute.
     *
     * If searchInPath is true, this just needs to be the file name.
     * Otherwise, it needs to be the absolute path of the file to execute.
     */
    std::string pathname;

    /**
     * Arguments for the process to execute.
     *
     * args[0] will be passed as the name of the child process
     */
    std::vector<std::string> args;

    /**
     * Custom environment for the child process, if envFromParent is false.
     */
    std::vector<std::string> env;

    /**
     * True if the environment is to be taken from the parent, false if it is
     * explicitly provided in env
     */
    bool envFromParent;

    /**
     * Set to true if the file is to be searched in the current $PATH.
     *
     * If this is set to true, the environment will always be taken from the
     * parent regardless of the values of envFromParent and env.
     */
    bool searchInPath;

    /// Create a new object that will execute program `program'
    Exec(const std::string& pathname)
        : pathname(pathname), envFromParent(true), searchInPath(false)
    {
        args.push_back(pathname);
    }

    /// Import the current environment into env
    void importEnv() {
        for (char** s = environ; *s; ++s)
            env.push_back(*s);
    }

    /// exec the program, never returning if all goes well
    void exec() {
        // Prepare the argument list
        char** exec_args = new char*[args.size() + 1];
        for (size_t i = 0; i < args.size(); ++i)
            exec_args[i] = strdup(args[i].c_str());
        exec_args[args.size()] = 0;

        char** exec_env = environ;
        if (!envFromParent)
        {
            // Prepare the custom environment
            exec_env = new char*[env.size() + 1];
            for (size_t i = 0; i < env.size(); ++i)
                // We can just store a pointer to the internal strings, since later
                // we're calling exec and no destructors will be called
                exec_env[i] = strdup(env[i].c_str());
            exec_env[env.size()] = 0;
        }

        if (searchInPath) {
            if (execvpe(pathname.c_str(), exec_args, exec_env) == -1)
                throw std::system_error( errno, std::system_category(), "trying to run " + pathname);
        } else {
            if (execve(pathname.c_str(), exec_args, exec_env) == -1)
                throw std::system_error( errno, std::system_category(), "trying to run " + pathname);
        }

        delete[] exec_args;
        if (exec_env != environ) delete[] exec_env;
        throw std::logic_error(
                "trying to run " + pathname + ": " +
                "Program flow continued after successful exec()" );
    }
};

/**
 * Execute a shell command using /bin/sh -c
 */
struct ShellCommand : Exec {
    ShellCommand(const std::string& cmd) :
#ifdef _WIN32
                Exec("bash") // let's hope for the best...
#else
                Exec("/bin/sh")
#endif
    {
        args.push_back("-c");
        args.push_back(cmd);
        searchInPath = false;
        envFromParent = true;
    }
};

struct Pipe {

    struct Writer : brick::shmem::Thread {
        int fd;
        bool close;
        std::string data;
        bool running;
        bool closed;
        std::mutex mutex;

        Writer() : fd( -1 ), close( false ), running( false ) {}

        void main() {
            do {
                int wrote = 0;

                {
                    std::lock_guard< std::mutex > __l( mutex );
                    wrote = ::write( fd, data.c_str(), data.length() );
                    if ( wrote > 0 )
                        data.erase( data.begin(), data.begin() + wrote );
                }

                if ( wrote == -1 ) {
                    if ( blocking( errno ) )
#ifndef _WIN32
                        sched_yield();
#else
                        ;
#endif
                    else
                        throw std::system_error( errno, std::system_category(), "writing to pipe" );
                }
            } while ( !done() );

            std::lock_guard< std::mutex > __l( mutex );
            running = false;
            if ( close )
                ::close( fd );
        }

        bool done() {
            std::lock_guard< std::mutex > __l( mutex );
            if ( data.empty() )
                running = false;
            return !running;
        }

        void run( int _fd, std::string what ) {
            std::lock_guard< std::mutex > __l( mutex );

            if ( running )
                ASSERT_EQ( _fd, fd );
            fd = _fd;
            ASSERT_NEQ( fd, -1 );

            data += what;
            if ( running )
                return;
            running = true;
            start();
        }
    };

    typedef std::deque< char > Buffer;
    Buffer buffer;
    int fd;
    bool _eof;
    Writer writer;

    Pipe( int p ) : fd( p ), _eof( false )
    {
        if ( p == -1 )
            return;
#ifndef _WIN32
        if ( fcntl( fd, F_SETFL, O_NONBLOCK ) == -1 )
            throw std::system_error( errno, std::system_category(), "fcntl on a pipe" );
#endif
    }
    Pipe() : fd( -1 ), _eof( false ) {}

    /* Writes data to the pipe, asynchronously. */
    void write( std::string what ) {
        writer.run( fd, what );
    }

    void close() {
        std::lock_guard< std::mutex > __l( writer.mutex );
        writer.close = true;
        if ( !writer.running )
            ::close( fd );
    }

    bool valid() {
        return fd != -1;
    }

    bool active() {
        return valid() && !eof();
    }

    bool eof() {
        return _eof;
    }

    static bool blocking( int err ) {
#ifndef _WIN32
        return err == EAGAIN || err == EWOULDBLOCK;
#else
    return err == EAGAIN;
#endif
    }

    int readMore() {
        ASSERT( valid() );
        char _buffer[1024];
        int r = ::read( fd, _buffer, 1023 );
        if ( r == -1 && !blocking( errno ) )
            throw std::system_error( errno, std::system_category(), "reading from pipe" );
        else if ( r == -1 )
            return 0;
        if ( r == 0 )
            _eof = true;
        else
            std::copy( _buffer, _buffer + r, std::back_inserter( buffer ) );
        return r;
    }

    std::string nextChunk() {
        std::string line( buffer.begin(), buffer.end() );
        buffer.clear();
        return line;
    }

    std::string nextLine() {
        ASSERT( valid() );
        Buffer::iterator nl =
            std::find( buffer.begin(), buffer.end(), '\n' );
        while ( nl == buffer.end() ) {
            if ( !readMore() )
                return ""; // would block, so give up
            nl = std::find( buffer.begin(), buffer.end(), '\n' );
        }
        std::string line( buffer.begin(), nl );

        if ( nl != buffer.end() )
            ++ nl;
        buffer.erase( buffer.begin(), nl );

        return line;
    }

    /* Only returns on eof() or when data is buffered. */
    void wait() {
        ASSERT( valid() );
#ifndef _WIN32
        fd_set fds;
        FD_ZERO( &fds );
#endif
        while ( buffer.empty() && !eof() ) {
            if ( readMore() )
                return;
            if ( eof() )
                return;
#ifndef _WIN32
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
            FD_SET( fd, &fds );
            select( fd + 1, &fds, 0, 0, 0 );
#pragma GCC diagnostic pop
#else
            sleep( 1 );
#endif
        }
    }
    std::string nextLineBlocking() {
        ASSERT( valid() );
        std::string l;
        while ( !eof() ) {
            l = nextLine();
            if ( !l.empty() )
                return l;
            if ( eof() )
                return std::string( buffer.begin(), buffer.end() );
            wait();
        }
        return l;
    }
};

struct PipeThrough {
    std::string cmd;

    PipeThrough( const std::string& _cmd ) : cmd( _cmd ) {}

    std::string run( std::string data ) {
        int _in, _out;

#ifdef _WIN32
        Exec exec(cmd);
#else
        ShellCommand exec(cmd);
#endif

        exec.setupRedirects( &_in, &_out, 0 );
        exec.fork();

        Pipe in( _in ), out( _out );

        in.write( data );
        in.close();
        std::string ret;
        while ( !out.eof() ) {
            out.wait();
            ret += out.nextChunk();
        }
        in.writer.join();
        return ret;
    }
};

}
}

namespace brick_test {
namespace process {
#ifdef _WIN32
#define sleep(x) Sleep((x) * 1000)
#endif

class EndlessChild : public brick::process::ChildProcess {
protected:
    int main() {
        while (true)
            sleep(60);
        return 0;
    }
};

class TestChild : public brick::process::ChildProcess {
protected:
    int main() {
        std::cout << "antani" << std::endl;
        return 0;
    }
};

inline std::string suckFd( int fd ) {
    std::string res;
    char c;
    while (true) {
        int r = read(fd, &c, 1);
        if (r == 0)
            break;
        if (r < 0)
            throw std::system_error( errno, std::system_category(), "reading data from file descriptor" );
        res += c;
    }
    return res;
}

struct TestChildprocess {

    // Try running the child process and kill it
    TEST(kill) {
#ifndef _WIN32
        EndlessChild child;

        // Start the child
        pid_t pid UNUSED = child.fork();

        // We should get a nonzero pid
        ASSERT(pid != 0);

        // Send SIGQUIT
        child.kill(2);

        // Wait for the child to terminate
        int res UNUSED = child.wait();

        // Check that it was indeed terminated by signal 2
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
        ASSERT(WIFSIGNALED(res));
        ASSERT_EQ(WTERMSIG(res), 2);
#pragma GCC diagnostic pop
#endif
    }

    // Try getting the output of the child process
    TEST(output) {
#ifndef _WIN32
        TestChild child;
        int out;

        // Fork the child redirecting its stdout
        pid_t pid UNUSED = child.forkAndRedirect(0, &out, 0);
        ASSERT(pid != 0);

        // Read the child output
        ASSERT_EQ(suckFd(out), "antani\n");

        // Wait for the child to terminate
        ASSERT_EQ(child.wait(), 0);
#endif
    }

    TEST(redirect) {
        brick::process::Exec child("echo");
        child.searchInPath = true;
        child.args.push_back("antani");
        int out;

        // Fork the child redirecting its stdout
        pid_t pid UNUSED = child.forkAndRedirect(0, &out, 0);
        ASSERT(pid != 0);

        // Read the child output
        ASSERT_EQ(suckFd(out), "antani\n");

        // Wait for the child to terminate
        ASSERT_EQ(child.wait(), 0);
    }

    TEST(shellCommand) {
        brick::process::ShellCommand child("A=antani; echo $A");
        int out;

        // Fork the child redirecting its stdout
        pid_t pid UNUSED = child.forkAndRedirect(0, &out, 0);
        ASSERT(pid != 0);

        // Read the child output
        ASSERT_EQ(suckFd(out), "antani\n");

        // Wait for the child to terminate
        ASSERT_EQ(child.wait(), 0);
    }

    TEST(inout) {
        brick::process::Exec child("cat");
        child.searchInPath = true;
        int in, out;

        // Fork the child redirecting its stdout
        child.forkAndRedirect(&in, &out, 0);
        // ASSERT(pid != 0);
        write(in, "hello\n", 6);
        close(in);

        // Read the child output
        ASSERT_EQ(suckFd(out), "hello\n");

        // Wait for the child to terminate
        ASSERT_EQ(child.wait(), 0);
    }

};

}
}

#endif
// vim: syntax=cpp tabstop=4 shiftwidth=4 expandtab
