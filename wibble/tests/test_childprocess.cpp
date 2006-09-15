#include <wibble/config.h>
#include <wibble/sys/process.h>
#include <wibble/sys/childprocess.h>
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
	char buf[10];
	read(out, buf, 7);
	ensure(memcmp(buf, "antani\n", 7) == 0);

	// Wait for the child to terminate
	ensure_equals(child.wait(), 0);
}

}

// vim:set ts=4 sw=4:
