#include "common/sysinfo.hh"

// {{{ vminfo 

vminfo_t::vminfo_t(){
  pid = getpid();
  snprintf (filename,sizeof filename, "/proc/%d/status", pid);
  vmsize=vmlck=vmrss=vmdata=vmstk=vmexe=vmlib=0;
  statm=false;
  char line[1000];

  // now detect the number of lines to be skipped
  FILE *statusfile;
  lineskip = 0;
  if ((statusfile = fopen (filename,"r")) != NULL) {
      vmsize=0;
      while (vmsize==0) {
          fscanf (statusfile,"VmSize:%d kB\nVmLck:%d kB\nVmRSS:%d kB\nVmData:%d kB\n\
VmStk:%d kB\nVmExe:%d kB\nVmLib:%d kB\n"
                  ,&vmsize,&vmlck,&vmrss,&vmdata,&vmstk,&vmexe,&vmlib);
          fgets(line, sizeof line, statusfile);
          lineskip++;
      }
      lineskip--;
      fclose(statusfile);
  }
}

vminfo_t::~vminfo_t()
{
}

void vminfo_t::print()
{
  scan();
  using namespace std;
  cout <<"VM: ";
  cout <<"size="<<vmsize<<" ";
  cout <<"rss="<<vmrss<<" ";
  if (!statm)
    {
      cout <<"lck="<<vmlck<<" ";
      cout <<"data="<<vmdata<<" ";
      cout <<"stk="<<vmstk<<" ";
      cout <<"exe="<<vmexe<<" ";
      cout <<"lib="<<vmlib<<" ";
    }
  cout <<endl;
}

void vminfo_t::scan()
{
#if defined(__linux__) || defined(_WIN32)
  char line[1000];
  FILE *statusfile;
  if ((statusfile = fopen (filename,"r")) == NULL) {
      return;
  }
  for (int i=0; i<lineskip; i++) {
      fgets (line, sizeof line, statusfile);
  }
  fscanf (statusfile,"VmSize:%d kB\nVmLck:%d kB\nVmRSS:%d kB\nVmData:%d kB\n\
VmStk:%d kB\nVmExe:%d kB\nVmLib:%d kB\n"
	  ,&vmsize,&vmlck,&vmrss,&vmdata,&vmstk,&vmexe,&vmlib);
  fclose(statusfile);
#elif defined(__APPLE__)
    task_t task = MACH_PORT_NULL;
    if (task_for_pid (current_task(), pid, &task) != KERN_SUCCESS) {
        perror ("task_for_pid");
        return;
    }
    struct task_basic_info task_basic_info;
    mach_msg_type_number_t task_basic_info_count = TASK_BASIC_INFO_COUNT;
    task_info(task, TASK_BASIC_INFO, (task_info_t)&task_basic_info,
              &task_basic_info_count);
    vmrss = task_basic_info.resident_size / 1024UL;
    unsigned long int vmsize_bytes = task_basic_info.virtual_size;
    if (vmsize_bytes > SHARED_TEXT_REGION_SIZE + SHARED_DATA_REGION_SIZE)
        vmsize_bytes -= SHARED_TEXT_REGION_SIZE + SHARED_DATA_REGION_SIZE;
    vmsize = vmsize_bytes / 1024UL;
#else
  // getrusage() stats are neither properly supported on Linux nor OSX
  // (2007-07-13)
  struct rusage rusage;
  if (getrusage (RUSAGE_SELF, &rusage) == -1) {
      perror ("getrusage");
      return;
  }
  vmrss  = rusage.ru_maxrss;
  // vmsize = ?
  vmdata = rusage.ru_idrss; //?
  vmstk  = rusage.ru_isrss; //?
  vmexe  = rusage.ru_ixrss; //?
#endif
}

int vminfo_t::getvmsize()
{
  scan();
  return vmsize;
}

int vminfo_t::getvmdata()
{
  scan();
  return vmdata;
}

int vminfo_t::getvmrss()
{
  scan();
  return vmrss;
}

// }}}
// {{{ timeinfo  
timeinfo_t::timeinfo_t()
{
  settimeout(0,0);
  reset();
}

timeinfo_t::timeinfo_t(double time)
{
  settimeout(time);
  reset();
}

timeinfo_t::timeinfo_t(long sec, long msec)
{
  settimeout(sec,msec);
  reset();
}

void timeinfo_t::settimeout(long sec,long msec)
{
  tv_usec =  1000*msec;
  tv_sec  =  sec + (tv_usec/1000000);
  tv_usec %= 1000000;
}

void timeinfo_t::settimeout(double time)
{
  tv_usec =  static_cast<long>(1000000*time)%1000000;
  tv_sec  =  static_cast<long>(time);
}

void timeinfo_t::reset()
{
  gettimeofday(&stored,0);
  stored.tv_usec += tv_usec;
  stored.tv_sec  += tv_sec + (stored.tv_usec/1000000);
  stored.tv_usec %= 1000000;
}

bool timeinfo_t::testtimeout()
{
  timeval current;
  gettimeofday(&current,0);
  if (stored.tv_sec < current.tv_sec ||
      (stored.tv_sec == current.tv_sec &&
       stored.tv_usec <= current.tv_usec))
    {
      return true;
    }
  return false;
}

void timeinfo_t::print_time(long sec,long microsec)
{
  using namespace std;
  cout <<sec+microsec/1000000<<".";
  if (microsec<10)
    cout <<"0";
  if (microsec<100)
    cout <<"0";
  if (microsec<1000)
    cout <<"0";
  if (microsec<10000)
    cout <<"0";
  if (microsec<100000)
    cout <<"0";
  cout<<microsec;
}

void timeinfo_t::print()
{  
  timeval current;
  gettimeofday(&current,0);
  long micro = 1000000 + current.tv_usec - (stored.tv_usec - tv_usec);
  long sec = current.tv_sec - 1 - (stored.tv_sec - tv_sec) ;
  sec += micro/1000000;
  micro %= 1000000;
  print_time(sec,micro);
}

double timeinfo_t::gettime()
{
  timeval current;
  gettimeofday(&current,0);
  long micro = 1000000 + current.tv_usec - (stored.tv_usec - tv_usec);
  long sec = current.tv_sec - 1 - (stored.tv_sec - tv_sec) ;
  sec += micro/1000000;
  micro %= 1000000;
  return (sec+(micro/1000000.0));
}

// }}}
// {{{ timeprofiler 

timeprofiler_t::timeprofiler_t(int n)
{
  timevalues.resize(n);
  for (int i=0; i<n; i++)
    {
      timevalues[i].tv_sec=0;
      timevalues[i].tv_usec=0;
    }
  focus=0;
  gettimeofday(&lasttick,0);
  firsttick = lasttick;
};

void timeprofiler_t::profile_off()
{
  profile_on(0);
}

void timeprofiler_t::profile_on(int n)
{
  timeval tick;
  gettimeofday(&tick,0);

  timevalues[focus].tv_sec += tick.tv_sec - lasttick.tv_sec;
  timevalues[focus].tv_usec += tick.tv_usec - lasttick.tv_usec;

  if (timevalues[focus].tv_usec>1000000)
    {
      timevalues[focus].tv_sec += timevalues[focus].tv_usec/1000000;
      timevalues[focus].tv_usec %= 1000000;
    }

  lasttick.tv_sec = tick.tv_sec;
  lasttick.tv_usec = tick.tv_usec;
  focus=n;
}

long timeprofiler_t::get_time_on(int n)
{
  timevalues[n].tv_sec += timevalues[n].tv_usec/1000000;
  timevalues[n].tv_usec = timevalues[n].tv_usec%1000000;
  return static_cast<long>
    (1000*(timevalues[n].tv_sec+timevalues[n].tv_usec/1000000.0));
}

long timeprofiler_t::get_global_time()
{
  timeval tick;
  gettimeofday(&tick,0);

  long micro = 1000000 + tick.tv_usec - firsttick.tv_usec;
  long sec = tick.tv_sec - 1 - firsttick.tv_sec;
  sec += micro/1000000;
  micro %= 1000000;

  return static_cast<long>(1000*(sec+micro/1000000.0));
}

// }}}
// {{{ loadinfo 

int loadinfo_t::getload()
{
  double result;
#if defined(__linux)
  getloadavg(&result,1);
#endif

#if defined(_WIN32)
  result = 0.0;
#endif 
  return static_cast<int>(100*result);
}

// }}}


