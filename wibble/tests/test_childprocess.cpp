#include <wibble/config.h>
#include <wibble/sys/process.h>
#include <wibble/sys/childprocess.h>
#include <wibble/sys/exec.h>
#include <cstdlib>
#include <iostream>

using namespace std;
using namespace wibble::sys;

#include <wibble/tests/tut-wibble.h>

namespace tut {

struct sys_childprocess_shar {};
TESTGRP( sys_childprocess );

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

// Try running the child process and kill it
template<> template<>
void to::test< 1 >() {
	EndlessChild child;

	// Start the child
	pid_t pid = child.fork();

	// We should get a nonzero pid
	ensure(pid != 0);

	// Send SIGQUIT
	child.kill(2);

	// Wait for the child to terminate
	int res = child.wait();

	// Check that it was indeed terminated by signal 2
	ensure(WIFSIGNALED(res));
	ensure_equals(WTERMSIG(res), 2);
}

// Try getting the output of the child process
template<> template<>
void to::test< 2 >() {
	TestChild child;
	int out;

	// Fork the child redirecting its stdout
	pid_t pid = child.forkAndRedirect(0, &out, 0);
	ensure(pid != 0);

	// Read the child output
	ensure_equals(suckFd(out), "antani\n");

	// Wait for the child to terminate
	ensure_equals(child.wait(), 0);
}

template<> template<>
void to::test< 3 >() {
	Exec child("/bin/echo");
	child.args.push_back("antani");
	int out;
	
	// Fork the child redirecting its stdout
	pid_t pid = child.forkAndRedirect(0, &out, 0);
	ensure(pid != 0);

	// Read the child output
	ensure_equals(suckFd(out), "antani\n");

	// Wait for the child to terminate
	ensure_equals(child.wait(), 0);
}

template<> template<>
void to::test< 4 >() {
	ShellCommand child("A=antani; echo $A");
	int out;
	
	// Fork the child redirecting its stdout
	pid_t pid = child.forkAndRedirect(0, &out, 0);
	ensure(pid != 0);

	// Read the child output
	ensure_equals(suckFd(out), "antani\n");

	// Wait for the child to terminate
	ensure_equals(child.wait(), 0);
}

}

// vim:set ts=4 sw=4:
