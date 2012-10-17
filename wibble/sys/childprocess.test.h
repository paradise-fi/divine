/* -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>
   (c) 2007 Enrico Zini <enrico@enricozini.org> */
#include <wibble/sys/childprocess.h>

#include <wibble/sys/process.h>
#include <wibble/sys/exec.h>
#include <cstdlib>
#include <unistd.h>
#include <iostream>
#include <unistd.h>

#include <wibble/test.h>


using namespace std;
using namespace wibble::sys;

#ifdef _WIN32
#define sleep Sleep
#endif

class EndlessChild : public ChildProcess
{
protected:
    int main()
    {
        while (true)
            sleep(60);
        return 0;
    }
};

class TestChild : public ChildProcess
{
protected:
    int main()
    {
        cout << "antani" << endl;
        return 0;
    }
};

std::string suckFd(int fd)
{
    std::string res;
    char c;
    while (true)
    {
        int r = read(fd, &c, 1);
        if (r == 0)
            break;
        if (r < 0)
            throw wibble::exception::System("reading data from file descriptor");
        res += c;
    }
    return res;
}

struct TestChildprocess {

    // Try running the child process and kill it
    Test kill() {
#ifdef POSIX
        EndlessChild child;

        // Start the child
        pid_t pid = child.fork();

        // We should get a nonzero pid
        assert(pid != 0);

        // Send SIGQUIT
        child.kill(2);

        // Wait for the child to terminate
        int res = child.wait();

        // Check that it was indeed terminated by signal 2
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
        assert(WIFSIGNALED(res));
        assert_eq(WTERMSIG(res), 2);
#pragma GCC diagnostic pop
#endif
    }

    // Try getting the output of the child process
    Test output() {
#ifdef POSIX
        TestChild child;
        int out;

        // Fork the child redirecting its stdout
        pid_t pid = child.forkAndRedirect(0, &out, 0);
        assert(pid != 0);

        // Read the child output
        assert_eq(suckFd(out), "antani\n");

        // Wait for the child to terminate
        assert_eq(child.wait(), 0);
#endif
    }

    Test redirect() {
        Exec child("echo");
        child.searchInPath = true;
        child.args.push_back("antani");
        int out;
	
        // Fork the child redirecting its stdout
        pid_t pid = child.forkAndRedirect(0, &out, 0);
        assert(pid != 0);

        // Read the child output
        assert_eq(suckFd(out), "antani\n");

        // Wait for the child to terminate
        assert_eq(child.wait(), 0);
    }

    Test shellCommand() {
        ShellCommand child("A=antani; echo $A");
        int out;
	
        // Fork the child redirecting its stdout
        pid_t pid = child.forkAndRedirect(0, &out, 0);
        assert(pid != 0);

        // Read the child output
        assert_eq(suckFd(out), "antani\n");

        // Wait for the child to terminate
        assert_eq(child.wait(), 0);
    }

    Test inout() {
        Exec child("cat");
        child.searchInPath = true;
        int in, out;
	
        // Fork the child redirecting its stdout
        child.forkAndRedirect(&in, &out, 0);
        // assert(pid != 0);
        write(in, "hello\n", 6);
        close(in);

        // Read the child output
        assert_eq(suckFd(out), "hello\n");

        // Wait for the child to terminate
        assert_eq(child.wait(), 0);
    }

};
// vim:set ts=4 sw=4:
