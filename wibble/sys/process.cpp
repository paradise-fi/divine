/*
 * OO base class for process functions and child processes
 *
 * Copyright (C) 2003-2008  Enrico Zini <enrico@debian.org>
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
#include <wibble/sys/process.h>

#ifdef POSIX
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

#include <cstdlib>
#include <sstream>

namespace wibble {
namespace sys {
namespace process {

using namespace std;

void detachFromTTY()
{
	int devnull = open("/dev/null", O_RDWR);
	if (devnull == -1) throw wibble::exception::File("/dev/null", "opening for read and write access");
	if (dup2(devnull, 0) == -1) throw wibble::exception::System("redirecting stdin to /dev/null");
	if (dup2(devnull, 1) == -1) throw wibble::exception::System("redirecting stdout to /dev/null");
	if (setsid() == -1) throw wibble::exception::System("trying to become session leader");
	if (dup2(devnull, 2) == -1) throw wibble::exception::System("redirecting stderr to /dev/null");
	close(devnull);
}

string formatStatus(int status)
{
	stringstream b_status;

	bool exited_normally = WIFEXITED(status);
	int exit_code = exited_normally ? WEXITSTATUS(status) : -1;
	bool dumped_core = status & 128;
	bool signaled = WIFSIGNALED(status);
	int signal = signaled ? WTERMSIG(status) : 0;

	if (exited_normally)
		if (exit_code == 0)
			b_status << "terminated successfully";
		else
			b_status << "exited with code " << exit_code;
	else
	{
		b_status << "was interrupted, killed by signal " << signal;
		if (dumped_core) b_status << " (core dumped)";
	}

	return b_status.str();
}

void chdir(const string& dir)
{
	if (::chdir(dir.c_str()) == -1)
		throw wibble::exception::System("changing working directory to " + dir);
}

std::string getcwd()
{
	size_t size = pathconf(".", _PC_PATH_MAX);
	char buf[size];
	if (::getcwd(buf, size) == NULL)
		throw wibble::exception::System("getting the current working directory");
	return buf;
}

void chroot(const string& dir)
{
	if (::chroot(dir.c_str()) == -1)
		throw wibble::exception::System("changing root directory to " + dir);
}

mode_t umask(mode_t mask)
{
	return ::umask(mask);
}

struct passwd* getUserInfo(const string& user)
{
	if (isdigit(user[0]))
		return getpwuid(atoi(user.c_str()));
	else
		return getpwnam(user.c_str());
}

struct group* getGroupInfo(const string& group)
{
	if (isdigit(group[0]))
		return getgrgid(atoi(group.c_str()));
	else
		return getgrnam(group.c_str());
}

void initGroups(const string& name, gid_t gid)
{
	if (::initgroups(name.c_str(), gid) == -1)
	{
		stringstream str;
		str << "initializing group access list for user " << name
			<< " with additional group " << gid;
		throw wibble::exception::System(str.str());
	}
}

static void set_perms(const string& user, uid_t uid, const string& group, gid_t gid)
{
	initGroups(user, gid);

	if (setgid(gid) == -1)
	{
		stringstream str;
		str << "setting group id to " << gid << " (" << group << ")";
		throw wibble::exception::System(str.str());
	}

	if (setegid(gid) == -1)
	{
		stringstream str;
		str << "setting effective group id to " << gid << " (" << group << ")";
		throw wibble::exception::System(str.str());
	}

	if (setuid(uid) == -1)
	{
		stringstream str;
		str << "setting user id to " << uid << " (" << user << ")";
		throw wibble::exception::System(str.str());
	}

	if (seteuid(uid) == -1)
	{
		stringstream str;
		str << "setting effective user id to " << uid << " (" << user << ")";
		throw wibble::exception::System(str.str());
	}
}

void setPerms(const string& user)
{
	struct passwd* pw = getUserInfo(user);
	if (!pw)
	{
		stringstream str;
		str << "User " << user << " does not exist on this system"; 
		throw wibble::exception::Consistency("setting process permissions", str.str());
	}
	struct group* gr = getgrgid(pw->pw_gid);
	if (!gr)
	{
		stringstream str;
		str << "Group " << pw->pw_gid << " (primary group of user "
			<< user << ") does not exist on this system";
		throw wibble::exception::Consistency("setting process permissions", str.str());
	}

	set_perms(user, pw->pw_uid, gr->gr_name, gr->gr_gid);
}

void setPerms(const string& user, const string& group)
{
	struct passwd* pw = getUserInfo(user);
	if (!pw)
	{
		stringstream str;
		str << "User " << user << " does not exist on this system";
		throw wibble::exception::Consistency("setting process permissions", str.str());
	}
	struct group* gr = getGroupInfo(group);
	if (!gr)
	{
		stringstream str;
		str << "Group " << group << " does not exist on this system";
		throw wibble::exception::Consistency("setting process permissions", str.str());
	}

	set_perms(user, pw->pw_uid, group, gr->gr_gid);
}

void setPerms(uid_t user)
{
	struct passwd* pw = getpwuid(user);
	if (!pw)
	{
		stringstream str;
		str << "User " << user << " does not exist on this system";
		throw wibble::exception::Consistency("setting process permissions", str.str());
	}
	struct group* gr = getgrgid(pw->pw_gid);
	if (!gr)
	{
		stringstream str;
		str << "Group " << pw->pw_gid
			<< " (primary group of user " << user << ") does not exist on this system";
		throw wibble::exception::Consistency("setting process permissions", str.str());
	}

	set_perms(pw->pw_name, pw->pw_uid, gr->gr_name, gr->gr_gid);
}

void setPerms(uid_t user, gid_t group)
       
{
	struct passwd* pw = getpwuid(user);
	if (!pw)
	{
		stringstream str;
		str << "User " << user << " does not exist on this system";
		throw wibble::exception::Consistency("setting process permissions", str.str());
	}
	struct group* gr = getgrgid(group);
	if (!gr)
	{
		stringstream str;
		str << "Group " << group << " does not exist on this system";
		throw wibble::exception::Consistency("setting process permissions", str.str());
	}

	set_perms(pw->pw_name, pw->pw_uid, gr->gr_name, gr->gr_gid);
}


static string describe_rlimit_res_t(int rlim)
{
	switch (rlim)
	{
		case RLIMIT_CPU: return "CPU time in seconds";
		case RLIMIT_FSIZE: return "Maximum filesize";
		case RLIMIT_DATA: return "max data size";
		case RLIMIT_STACK: return "max stack size";
		case RLIMIT_CORE: return "max core file size";
		case RLIMIT_RSS: return "max resident set size";
		case RLIMIT_NPROC: return "max number of processes";
		case RLIMIT_NOFILE: return "max number of open files";
		case RLIMIT_MEMLOCK: return "max locked-in-memory address spac";
#ifndef __APPLE__
		case RLIMIT_AS: return "address space (virtual memory) limit";
#endif
		default: return "unknown";
	}
}

static void setLimit(int rlim, int val)
{
	struct rlimit lim;
	if (getrlimit(rlim, &lim) == -1)
		throw wibble::exception::System("Getting " + describe_rlimit_res_t(rlim) + " limit");
	lim.rlim_cur = val;
	if (setrlimit(rlim, &lim) == -1)
	{
		stringstream str;
		str << "Setting " << describe_rlimit_res_t(rlim) << " limit to " << val;
		throw wibble::exception::System(str.str());
	}
}

static int getLimit(int rlim, int* max = 0)
{
	struct rlimit lim;
	if (getrlimit(rlim, &lim) == -1)
		throw wibble::exception::System("Getting " + describe_rlimit_res_t(rlim) + " limit");
	if (max)
		*max = lim.rlim_max;
	return lim.rlim_cur;
}

int getCPUTimeLimit(int* max) { return getLimit(RLIMIT_CPU, max); }
int getFileSizeLimit(int* max) { return getLimit(RLIMIT_FSIZE, max); }
int getDataMemoryLimit(int* max) { return getLimit(RLIMIT_DATA, max); }
int getCoreSizeLimit(int* max) { return getLimit(RLIMIT_CORE, max); }
int getChildrenLimit(int* max) { return getLimit(RLIMIT_NPROC, max); }
int getOpenFilesLimit(int* max) { return getLimit(RLIMIT_NOFILE, max); }

void setCPUTimeLimit(int value) { setLimit(RLIMIT_CPU, value); }
void setFileSizeLimit(int value) { setLimit(RLIMIT_FSIZE, value); }
void setDataMemoryLimit(int value) { setLimit(RLIMIT_DATA, value); }
void setCoreSizeLimit(int value) { setLimit(RLIMIT_CORE, value); }
void setChildrenLimit(int value) { setLimit(RLIMIT_NPROC, value); }
void setOpenFilesLimit(int value) { setLimit(RLIMIT_NOFILE, value); }

}
}
}
#endif
// vim:set ts=4 sw=4:
