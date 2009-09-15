#ifndef _DIVINE_SYSINFO_HH_
#define _DIVINE_SYSINFO_HH_

#include <iostream>

// vminfo
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#if defined (__linux)
#include <sys/resource.h>
#endif
#ifdef __APPLE__
#include <mach/task.h>
#include <mach/mach_init.h>
#include <mach/shared_memory_server.h>
#endif
#include <fcntl.h>
#include <stdio.h>

// timeprofiler
#include <vector>

// loadinfo
#include <stdlib.h>

//!Class for a memory consumtion measuring
class vminfo_t {
protected:
  bool statm;
  int pid;
  char filename[100];

  int lineskip;

public:
  int vmsize,vmlck,vmrss,vmdata,vmstk,vmexe,vmlib;

  vminfo_t();
  ~vminfo_t();
  void scan();  
  int getvmsize();
  int getvmrss();
  int getvmdata();
  void print();
};

//!Class for a time measuring
class timeinfo_t
{
protected:
  timeval stored;
  long tv_sec;
  long tv_usec;
  void print_time(long,long);
public:
  timeinfo_t();
  timeinfo_t(double);
  timeinfo_t(long,long);
  void settimeout(long,long);  // seconds, microseconds
  void settimeout(double);     // seconds (floating point)
  void reset();
  bool testtimeout();
  double gettime();
  void print();
};

class timeprofiler_t {
private:
  std::vector<timeval> timevalues;
  timeval lasttick,firsttick;
  int focus;
public:
  timeprofiler_t(int = 100);
  void profile_on(int);
  void profile_off();
  long get_time_on(int);
  long get_global_time();
};


class loadinfo_t {
public:
  int getload();
};

#endif


