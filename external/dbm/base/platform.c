/* -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; -*-
 *
 * This file is part of the UPPAAL DBM library.
 *
 * The UPPAAL DBM library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * The UPPAAL DBM library is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with the UPPAAL DBM library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA.
 */

/* -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/*********************************************************************
 *
 * Filename : platform.c
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * v 1.2 reviewed.
 * $Id: platform.c,v 1.5 2004/04/02 22:50:43 behrmann Exp $
 *
 *********************************************************************/

#include "base/platform.h"

#include <time.h>
#include <ctype.h>
#include <string.h>

/* GCC -- ANSI C */

#ifdef __GNUC__

#include <stdio.h>
#include <stdlib.h>

/* clock() returns the CPU time consumed by the process measured in
 * number of clock ticks. It will return -1 in case of errors, but we
 * cannot do anything in that case, so we simply ignore it.
 */
double base_getTime()
{
    return (double) clock() / (double) CLOCKS_PER_SEC;
}

#elif defined(_WIN32)

/* Microsoft C++ -- not ANSI
 * clock() is defined but doesn't
 * work as expected so use something
 * else.
 */

#include <sys/types.h>
#include <sys/timeb.h>

double base_getTime()
{
    struct _timeb now;
    _ftime(&now);
    /* seconds + millisecons * 1e-3 */
    return (double)now.time + (double)(now.millitm * 1e-3);
}

#endif

#ifdef __MINGW32__
/* strerror is inconsistent with errno.h numbers on MinGW32 (only?) */
const char* oserror(int error_code)
{
  DWORD dwError = error_code;
  char* lpMsgBuf;
  if (!FormatMessage(
		     FORMAT_MESSAGE_ALLOCATE_BUFFER |
		     FORMAT_MESSAGE_FROM_SYSTEM |
		     FORMAT_MESSAGE_IGNORE_INSERTS,
		     NULL,
		     dwError,
		     MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
		     (LPTSTR) &lpMsgBuf,
		     0,
		     NULL ))
    {
      return "(error message not available)";
    } else {
      char *eol = lpMsgBuf; // remove end of lines from M$ idiocracies:
      while (NULL != (eol=strchr(eol,'\n'))) {
	if (eol[1]==0) eol[0] = 0;
	else *eol = '.';
      }
      return lpMsgBuf;
    }
}

#else /* UNIX */

#include <string.h>
const char* oserror(int error_code) { return strerror(error_code); }

#endif


#ifdef __MINGW32__

void base_getMemInfo(meminfo_t info)
{
    /* Note: GetPerformanceInfo and GetSystemFileCacheSize may provide more info
     * about RAM usage (i.e. caches and buffers), but they are accessible only on 
     * XP and later Windows, hence nobody knows what would happen on 2000 or 
     * earlier :-(
     */

    MEMORYSTATUS status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatus(&status);
    
    info[PHYS_TOTAL] = status.dwTotalPhys >> 10;
    info[PHYS_AVAIL] = status.dwAvailPhys >> 10;
    info[PHYS_CACHE] = 0;
    info[SWAP_TOTAL] = status.dwTotalPageFile >> 10;
    info[SWAP_AVAIL] = status.dwAvailPageFile >> 10;
    info[VIRT_TOTAL] = status.dwTotalVirtual >> 10;
    info[VIRT_AVAIL] = status.dwAvailVirtual >> 10;
}

#elif __linux__

static const char* info_tags[6] =
{
    "MemTotal",  /* PHYS_TOTAL */
    "MemFree",   /* PHYS_AVAIL */
    "Buffers",   /* PHYS_CACHE */
    "Cached",    /* PHYS_CACHE */
    "SwapTotal", /* SWAP_TOTAL */
    "SwapFree"   /* SWAP_AVAIL */
};

void base_getMemInfo(meminfo_t info)
{
    /* based on Linux/Documentation/filesystems/proc.txt#meminfo */
    FILE *meminfo = fopen("/proc/meminfo", "r");
    int start, end, i, ok = 0;
    uint64_t tmp;

    if (meminfo)
    {
        char buffer[256];
        while(ok < 6 && fgets(buffer, sizeof(buffer), meminfo))
        {
            /* Extract the key. */
            for(end = 0; end < sizeof(buffer) && buffer[end] != ':'; ++end);

            /* Find a match with the tags. */
            for(i = ok; i < 6; ++i)
            {
                if (strncmp(info_tags[i], buffer, end) == 0)
                {
                    /* Extract the value. */
                    for(start = end+1; start < sizeof(buffer) && (buffer[start] < '0' || buffer[start] > '9'); ++start);
                    for(end = start+1; end < sizeof(buffer) && buffer[end] >= '0' && buffer[end] <= '9'; ++end);
                    buffer[end] = '\0';
                    if (start >= sizeof(buffer))
                    {
                        break;
                    }
                    tmp = atoll(&buffer[start]);

                    /* The unit is just after. */
                    for(start = end+1; start < sizeof(buffer) && (buffer[start] == ' ' || buffer[start] == '\t'); ++start);
                    for(end = start+1; end < sizeof(buffer) && buffer[end] != '\n' && buffer[end] !='\0'; ++end);

                    if (end < sizeof(buffer))
                    {
                        buffer[end] = '\0';
                        /* adjust value to kilo bytes */
                        if (strcasecmp("b", buffer+start) == 0) 
                        {
                            tmp >>= 10;
                        }
                        else if (strcasecmp("mb", buffer+start) == 0) 
                        {
                            tmp <<= 10;
                        } 
                        else if (strcasecmp("gb", buffer+start) == 0) 
                        {
                            tmp <<= 20;
                        }
                    }

                    if (i < 2) 
                    {
                        info[i] = tmp;
                    } 
                    else if (i == 2)
                    { /* buffer memory should be added to cache */
                        info[i-1] += tmp;
                    } 
                    else
                    { /* the rest are shifted */
                        info[i-1] = tmp;
                    }
                    ok++;
                    break;
                }
            }
        }
        fclose(meminfo);
        ok = (ok == 6);
    }

    if (ok)
    {
        info[VIRT_TOTAL] = info[PHYS_TOTAL] + info[SWAP_TOTAL];
        info[VIRT_AVAIL] = info[PHYS_AVAIL] + info[SWAP_AVAIL];
    }
    else
    {
        for(i = 0; i < 6; ++i)
        {
            info[i] = 0;
        }
    }
}

#elif defined(__APPLE__) && defined(__MACH__)

#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/host_info.h>
#include <mach/mach_host.h>

void base_getMemInfo(meminfo_t info)
{

    int mib[2];
    int memory;
    struct xsw_usage swapu;
    size_t len;

    // Total physical memory
    mib[0] = CTL_HW;
    mib[1] = HW_PHYSMEM;
    len = sizeof(memory);
    sysctl(mib, 2, &memory, &len, NULL, 0); 
    info[PHYS_TOTAL] = memory / 1024; // in kB
    
    // Cached memory 
    info[PHYS_CACHE] = 0;

    // Swap
    mib[0] = CTL_VM;
    mib[1] = VM_SWAPUSAGE;
    len = sizeof(swapu);
    sysctl(mib, 2, &swapu, &len, NULL, 0); 
    info[SWAP_TOTAL] = swapu.xsu_total / 1024; // in kB
    info[SWAP_AVAIL] = swapu.xsu_avail / 1024; // in kB
    
    info[VIRT_TOTAL] = info[PHYS_TOTAL] + info[SWAP_TOTAL];
    info[VIRT_AVAIL] = info[PHYS_AVAIL] + info[SWAP_AVAIL];
    
    // Free memory 
    struct vm_statistics page_info;
    vm_size_t pagesize;
    mach_msg_type_number_t count;
    kern_return_t kret;

    pagesize = 0;
    kret = host_page_size (mach_host_self(), &pagesize);

    // vm stats
    count = HOST_VM_INFO_COUNT;
    kret = host_statistics (mach_host_self(), HOST_VM_INFO, (host_info_t)&page_info, &count);
    if (kret == KERN_SUCCESS)
        info[PHYS_AVAIL]  = page_info.free_count*pagesize / 1024;
    else 
        info[PHYS_AVAIL]  = 0;
    
}

#endif

#ifdef __MINGW32__

#include <stdint.h>
#include <windows.h>
#include <psapi.h>

static HANDLE hThisProcess = NULL;

void base_initProcInfo()
{
    hThisProcess = GetCurrentProcess();
}

void base_getProcInfo(procinfo_t info)
{
    FILETIME now;
    LARGE_INTEGER li;
    FILETIME ftCreationTime;
    FILETIME ftExitTime;
    FILETIME ftKernelTime;
    FILETIME ftUserTime;
    PROCESS_MEMORY_COUNTERS pmc = { sizeof(pmc) };

    GetSystemTimeAsFileTime(&now);
    GetProcessMemoryInfo(hThisProcess, &pmc, sizeof(pmc));
    if (GetProcessTimes(hThisProcess, &ftCreationTime, &ftExitTime,
                        &ftKernelTime, &ftUserTime)) {
        li.LowPart = ftUserTime.dwLowDateTime;
        li.HighPart = ftUserTime.dwHighDateTime;
        info[PROC_TIME_USR] = li.QuadPart / 10000;// 100 nano-secs to millis
        
        li.LowPart = ftKernelTime.dwLowDateTime;
        li.HighPart = ftKernelTime.dwHighDateTime;
        info[PROC_TIME_SYS] = li.QuadPart / 10000; // 100 nano-secs to millis
    }    
    info[PROC_MEM_VIRT] = (pmc.WorkingSetSize+pmc.PagefileUsage) >> 10;
    info[PROC_MEM_WORK] = pmc.WorkingSetSize >> 10;
    info[PROC_MEM_SWAP] = pmc.PagefileUsage >> 10;
    
    li.LowPart = now.dwLowDateTime;
    li.HighPart = now.dwHighDateTime;
    info[PROC_TIME_TSP] = li.QuadPart / 10000; // to milliseconds    
}

#elif __linux__

#include <inttypes.h>
#include <sys/param.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

inline void readNthToken(FILE* f, int n, char* buffer, int max)
{
    int i = 0, c = ' ';
    char buf[max+1];
    while (n != 0) 
    { /* skip N tokens */
        /* skip whitespace */
        while (c != EOF && c == ' ') c = fgetc(f);
        /* skip token */
        i = 0;
        while (c != EOF && c != ' ') {
            buf[i++] = c ;
            c = fgetc(f);
        }
        buf[i] = '\0';
        n--;
    }        
    /* skip whitespace */
    while (c != EOF && c == ' ') c = fgetc(f);
    /* read token */
    i = 0;
    while (c != EOF && c != ' ' && i<max) 
    {
        buffer[i] = c;
        c = fgetc(f);
        i++;
    }
    buffer[i] = '\0';
}

static int msecs_per_tick = 0;
static int pagesize = 0;

void base_initProcInfo()
{
    msecs_per_tick = 1000LLU / sysconf(_SC_CLK_TCK);
    pagesize = sysconf(_SC_PAGESIZE);
}

void base_getProcInfo(procinfo_t info)
{
    struct timeval now;
    char buf[64];
    gettimeofday(&now, NULL);
    FILE *ps = fopen("/proc/self/stat", "r");

    if (ps != NULL) {
        /* PID
           filename of executable
           state
           parent PID
           process group
           session
           tty nr
           process group ID of tty owner
           flags
           No. minor faults
           No. minor faults in children
           No. major faults
           No. major faults in children */
        readNthToken(ps, 13, buf, 63); /* No. jiffies in user mode */
        info[PROC_TIME_USR] = atoll(buf);
        readNthToken(ps, 0, buf, 63); /* No. jiffies in kernel mode */
        info[PROC_TIME_SYS] = atoll(buf);
        readNthToken(ps, 0, buf, 63); /* children jiffies in user mode */
        info[PROC_TIME_USR] += atoll(buf);
        info[PROC_TIME_USR] *= msecs_per_tick;
        readNthToken(ps, 0, buf, 63); /* children jiffies in kernel mode */
        info[PROC_TIME_SYS] += atoll(buf);
        info[PROC_TIME_SYS] *= msecs_per_tick;
        /* priority
           nice value
           0 (zero)
           jiffies before next SIGALARM
           jiffies until start (since boot) */
        readNthToken(ps, 5, buf, 63); /* virtual memory in bytes */
        info[PROC_MEM_VIRT] = atoll(buf);        
        readNthToken(ps, 0, buf, 63); /* resident set size in pages(?) */
        info[PROC_MEM_WORK] = atoll(buf);
        info[PROC_MEM_WORK] *= pagesize;
        /* current limit of rss
           startcode address
           endcode address
           startstack address
           current value of stack pointer
           current value of instruction pointer
           bitmap of pending signals
           bitmap of blocked signals
           bitmap of ignored signals
           bitmap of caught signals
           channel on which the process is waiting */
        readNthToken(ps, 11, buf, 63); /* no. swapped pages (not maintained) */
        info[PROC_MEM_SWAP] = atoll(buf);                
        readNthToken(ps, 0, buf, 63); /* children swapped pages (not maintained) */
        info[PROC_MEM_SWAP] += atoll(buf);
        info[PROC_MEM_SWAP] *= pagesize;
        /* convert to KB */
        info[PROC_MEM_VIRT] >>= 10; 
        info[PROC_MEM_WORK] >>= 10; 
        info[PROC_MEM_SWAP] >>= 10;
        fclose(ps);
    }
    info[PROC_TIME_TSP] = 1000LLU * now.tv_sec + now.tv_usec / 1000;
}

#elif defined(__APPLE__) && defined(__MACH__)

#include <sys/time.h>
#include <sys/resource.h>
#include <mach/task.h>
#include <mach/task_info.h>

void base_initProcInfo()
{
    // Nothing
}

void base_getProcInfo(procinfo_t info)
{
    struct timeval now;
    gettimeofday(&now, NULL);

    info[PROC_TIME_TSP] = 1000LLU * now.tv_sec + now.tv_usec / 1000;

    /*
    struct rusage res;
    getrusage(RUSAGE_SELF, &res);

    // Times in ms
    info[PROC_TIME_USR] = 1000LLU * res.ru_utime.tv_sec + res.ru_utime.tv_usec / 1000;
    info[PROC_TIME_SYS] = 1000LLU * res.ru_stime.tv_sec + res.ru_stime.tv_usec / 1000;

    // Memory in kB
    info[PROC_MEM_VIRT] = 0;    // To be done
    info[PROC_MEM_WORK] = res.ru_maxrss;
    info[PROC_MEM_SWAP] = 0;    // To be done
    */

    // Process stats
    struct task_basic_info tinfo;
    struct task_thread_times_info thinfo;
    mach_msg_type_number_t count;
    kern_return_t kret;

    count = TASK_BASIC_INFO_COUNT;
    kret = task_info (mach_task_self(), TASK_BASIC_INFO, (task_info_t)&tinfo, &count);
    if (kret == KERN_SUCCESS)
    {
        info[PROC_MEM_VIRT] = tinfo.virtual_size / 1024;
        info[PROC_MEM_WORK] = tinfo.resident_size / 1024;
        info[PROC_MEM_SWAP] = 0;
    }

    count = TASK_THREAD_TIMES_INFO_COUNT;
    kret = task_info (mach_task_self(), TASK_THREAD_TIMES_INFO, (task_info_t)&thinfo, &count);
    if (kret == KERN_SUCCESS)
    {
        info[PROC_TIME_USR] = 1000LLU * thinfo.user_time.seconds + thinfo.user_time.microseconds / 1000;
        info[PROC_TIME_SYS] = 1000LLU * thinfo.system_time.seconds + thinfo.system_time.microseconds / 1000;
    }
}

#endif





//#ifdef __MINGW32__

/* Origianl source found there:
http://social.msdn.microsoft.com/Forums/en-US/vcgeneral/thread/25a654f9-b6b6-490a-8f36-c87483bb36b7/
No license mentioned. Fixed year base, reformated, and simplified.
*/
 
#define TM_YEAR_BASE   1900
 
static int conv_num(const char **, int *, int, int);
 
static const char *day[7] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday",
    "Friday", "Saturday"
};
static const char *abday[7] = {
    "Sun","Mon","Tue","Wed","Thu","Fri","Sat"
};
static const char *mon[12] = {
    "January", "February", "March", "April", "May", "June", "July",
    "August", "September", "October", "November", "December"
};
static const char *abmon[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};
static const char *am_pm[2] = {
    "AM", "PM"
};
 
 
char* strptime2(const char *buf, const char *fmt, struct tm *tm, time_t *now)
{
    char c;
    const char *bp;
    size_t len = 0;
    int i, split_year = 0;
 
    bp = buf;
    memset(tm, 0, sizeof(struct tm));

    while ((c = *fmt) != '\0')
    {
        /* Eat up white-space. */
        if (isspace(c))
        {
            while (isspace(*bp)) bp++;
 
            fmt++;
            continue;
        }
                 
        if ((c = *fmt++) != '%') goto literal;
 
        switch (c = *fmt++)
        {
        case '%': /* "%%" is converted to "%". */
        literal:
            if (c != *bp++) return NULL;
            break;
            
        case 'E':
            // ignore.
            break;
 
        case 'O':
            // ignore.
            break;

            /*
             * "Complex" conversion rules, implemented through recursion.
             */
        case 'c': /* Date and time, using the locale's format. */
            if (!(bp = strptime2(bp, "%x %X", tm, now))) return NULL;
            break;
 
        case 'D': /* The date as "%m/%d/%y". */
            if (!(bp = strptime2(bp, "%m/%d/%y", tm, now))) return NULL;
            break;
 
        case 'R': /* The time as "%H:%M". */
            if (!(bp = strptime2(bp, "%H:%M", tm, now))) return NULL;
            break;
         
        case 'r': /* The time in 12-hour clock representation. */
            if (!(bp = strptime2(bp, "%I:%M:%S %p", tm, now))) return NULL;
            break;
 
        case 'T': /* The time as "%H:%M:%S". */
            if (!(bp = strptime2(bp, "%H:%M:%S", tm, now))) return NULL;
            break;
 
        case 'X': /* The time, using the locale's format. */
            if (!(bp = strptime2(bp, "%H:%M:%S", tm, now))) return NULL;
            break;
 
        case 'x': /* The date, using the locale's format. */
            if (!(bp = strptime2(bp, "%m/%d/%y", tm, now))) return NULL;
            break;
 
            /*
             * "Elementary" conversion rules.
             */
        case 'A': /* The day of week, using the locale's form. */
        case 'a':
            for (i = 0; i < 7; i++)
            {
                /* Full name. */
                len = strlen(day[i]);
                if (strncasecmp((char *)(day[i]), (char *)bp, len) == 0) break;
         
                /* Abbreviated name. */
                len = strlen(abday[i]);
                if (strncasecmp((char *)(abday[i]), (char *)bp, len) == 0) break;
            }
 
            /* Nothing matched. */
            if (i == 7) return NULL;
 
            tm->tm_wday = i;
            bp += len;
            break;
 
        case 'B': /* The month, using the locale's form. */
        case 'b':
        case 'h':
            for (i = 0; i < 12; i++)
            {
                /* Full name. */
                
                len = strlen(mon[i]);
                if (strncasecmp((char *)(mon[i]), (char *)bp, len) == 0) break;
 
                /* Abbreviated name. */
                len = strlen(abmon[i]);
                if (strncasecmp((char *)(abmon[i]),(char *) bp, len) == 0) break;
            }
 
            /* Nothing matched. */
            if (i == 12) return NULL;
 
            tm->tm_mon = i;
            bp += len;
            break;
 
        case 'C': /* The century number. */
            if (!(conv_num(&bp, &i, 0, 99))) return NULL;
 
            if (split_year)
            {
                tm->tm_year = (tm->tm_year % 100) + (i * 100);
            }
            else
            {
                tm->tm_year = i * 100;
                split_year = 1;
            }
            break;
 
        case 'd': /* The day of month. */
        case 'e':
            if (!(conv_num(&bp, &tm->tm_mday, 1, 31))) return NULL;
            break;
 
        case 'k': /* The hour (24-hour clock representation). */
            /* FALLTHROUGH */
        case 'H':
            if (!(conv_num(&bp, &tm->tm_hour, 0, 23))) return NULL;
            break;
 
        case 'l': /* The hour (12-hour clock representation). */
            /* FALLTHROUGH */
        case 'I':
            if (!(conv_num(&bp, &tm->tm_hour, 1, 12))) return NULL;
            if (tm->tm_hour == 12) tm->tm_hour = 0;
            break;
         
        case 'j': /* The day of year. */
            if (!(conv_num(&bp, &i, 1, 366))) return NULL;
            tm->tm_yday = i - 1;
            break;
         
        case 'M': /* The minute. */
            if (!(conv_num(&bp, &tm->tm_min, 0, 59))) return NULL;
            break;
         
        case 'm': /* The month. */
            if (!(conv_num(&bp, &i, 1, 12))) return NULL;
            tm->tm_mon = i - 1;
            break;
 
        case 'p': /* The locale's equivalent of AM/PM. */
            /* AM? */
            if (strcasecmp(am_pm[0], bp) == 0)
            {
                if (tm->tm_hour > 11) return NULL;

                bp += strlen(am_pm[0]);
                break;
            }
            /* PM? */
            else if (strcasecmp(am_pm[1], bp) == 0)
            {
                if (tm->tm_hour > 11) return NULL;

                tm->tm_hour += 12;
                bp += strlen(am_pm[1]);
                break;
            }
            /* Nothing matched. */
            return NULL;
         
        case 'S': /* The seconds. */
            if (!(conv_num(&bp, &tm->tm_sec, 0, 61))) return NULL;
            break;
         
        case 'U': /* The week of year, beginning on sunday. */
        case 'W': /* The week of year, beginning on monday. */
            /*
             * XXX This is bogus, as we can not assume any valid
             * information present in the tm structure at this
             * point to calculate a real value, so just check the
             * range for now.
             */
            if (!(conv_num(&bp, &i, 0, 53))) return NULL;
            break;
 
        case 'w': /* The day of week, beginning on sunday. */
            if (!(conv_num(&bp, &tm->tm_wday, 0, 6))) return NULL;
            break;
 
        case 'Y': /* The year. */
            if (!(conv_num(&bp, &i, 0, 9999))) return NULL;
 
            tm->tm_year = i - TM_YEAR_BASE;
            break;
         
        case 'y': /* The year within 100 years of the epoch. */
            if (!(conv_num(&bp, &i, 0, 99))) return NULL;
 
            if (split_year)
            {
                tm->tm_year = ((tm->tm_year / 100) * 100) + i;
                break;
            }
            split_year = 1;
            if (i <= 68)
            {
                tm->tm_year = i + 2000 - TM_YEAR_BASE;
            }
            else
            {
                tm->tm_year = i + 1900 - TM_YEAR_BASE;
            }
            break;
 
            /*
             * Miscellaneous conversions.
             */
        case 'n': /* Any kind of white-space. */
        case 't':
            while (isspace(*bp)) bp++;
            break;

            /* Time zone - shift "now" to this time zone
             * to make comparisons possible.
             */
        case 'z':
            tzset();
            if (!conv_num(&bp, &i, 0, 2400)) return NULL;
            i = (i % 100) * 60 + (i / 100) * 3600 + timezone; // in sec
            *now += i;
            break;
 
        default: /* Unknown/unsupported conversion. */
            return NULL;
        }
    }

    return (char*) bp;
}
 
static int conv_num(const char **buf, int *dest, int llim, int ulim)
{
    int result = 0;
    int neg = 0;

    /* The limit also determines the number of valid digits. */
    int rulim = ulim;
 
    if (**buf == '+')
    {
        (*buf)++;
    }
    else if (**buf == '-')
    {
        neg = 1;
        (*buf)++;
    }
    if (**buf < '0' || **buf > '9') return 0;
 
    do {
        result *= 10;
        result += *(*buf)++ - '0';
        rulim /= 10;
    } while ((result * 10 <= ulim) && rulim && **buf >= '0' && **buf <= '9');
 
    if (result < llim || result > ulim) return 0;
 
    *dest = neg ? -result : result;
    return 1;
}

//#endif

#ifdef __MINGW32__

// Windows.
// Revised code not using NetBios.
// http://stackoverflow.com/questions/221894/c-get-mac-address-of-network-adapters-on-vista

#include <windows.h>
#include <iphlpapi.h>

maclist_t* base_getMAC()
{
    IP_ADAPTER_INFO AdapterInfo[64];              // Allocate information for up to 64 NICs
    PIP_ADAPTER_INFO pAdapterInfo = AdapterInfo;
    DWORD dwBufLen = sizeof(AdapterInfo);         // Save memory size of buffer
    DWORD dwStatus = GetAdaptersInfo(AdapterInfo, // [out] buffer to receive data
                                     &dwBufLen);  // [in] size of receive data buffer
    int i = 0, j;
    maclist_t *res;
  
    //No network card? Other error?
    if(dwStatus != ERROR_SUCCESS)
    {
        return NULL;
    }

    // Count.
    for(pAdapterInfo = AdapterInfo; pAdapterInfo != NULL; pAdapterInfo = pAdapterInfo->Next)
    {
        if(pAdapterInfo->Type == MIB_IF_TYPE_ETHERNET)
        {
            ++i;
        }
    }

    // No MAC found.
    if (i == 0)
    {
        return NULL;
    }

    // Allocate.
    res = (maclist_t*) malloc(sizeof(maclist_t) +
                              i * sizeof(mac_adr_t));
    if (!res)
    {
        return NULL;
    }
    res->size = i;
    i = 0;

    // Write MACs.
    for(pAdapterInfo = AdapterInfo; pAdapterInfo != NULL; pAdapterInfo = pAdapterInfo->Next)
    {
        if(pAdapterInfo->Type == MIB_IF_TYPE_ETHERNET)
        {
            for(j = 0; j < 6; ++j)
            {
                res->mac[i][j] = pAdapterInfo->Address[j];
            }
            ++i;
        }
    }
    
    return res;
}

#elif defined(__APPLE__) && defined(__MACH__)

// Mac OS X

#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/errno.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/ethernet.h>
#include <sys/types.h>
#include <sys/sysctl.h> 

maclist_t* base_getMAC()
{
    struct ifreq *IFR;
    struct ifconf ifc;
    char buf[1024];
    int s, i, len;
    maclist_t *res;

    // Create a socket
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
    {
        return NULL;
    }

    // Get info on all available interfaces
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if (ioctl(s, SIOCGIFCONF, &ifc) < 0)
    {
        return NULL;
    }

    len = ifc.ifc_len / sizeof(struct ifreq);
    res = (maclist_t*) malloc(sizeof(maclist_t) +
                              len * sizeof(mac_adr_t));
    if (!res)
    {
        return NULL;
    }
    res->size = 0;

    i = 0;
    while (i < ifc.ifc_len)
    {
        IFR = (struct ifreq*) (((char*) ifc.ifc_req ) + i); 
        i += sizeof(IFR->ifr_name) +
            (IFR->ifr_addr.sa_len > sizeof(struct sockaddr)
             ? IFR->ifr_addr.sa_len : sizeof(struct sockaddr)); 

        // It is an internetwork interface
        if (IFR->ifr_addr.sa_family != AF_INET)
        {
            continue;
        }

        // The interface is up
        if (ioctl(s, SIOCGIFFLAGS, IFR) < 0)
        {
            continue;
        }
        
        if (!(IFR->ifr_flags & IFF_UP))
        {
            continue;
        }

        // It is not the Loopback interface
        if (IFR->ifr_flags & IFF_LOOPBACK)
        {
            continue;
        }

        int mib[6];
    
        mib[0] = CTL_NET;
        mib[1] = AF_ROUTE;
        mib[2] = 0;
        mib[3] = AF_LINK;
        mib[4] = NET_RT_IFLIST;
        mib[5] = if_nametoindex(IFR->ifr_name);
        
        if (mib[5] == 0)
        {
            return NULL;
        }
        
        if (sysctl(mib, 6, NULL, (size_t*)&len, NULL, 0) < 0)
        {
            return NULL;
        }
        
        char * macbuf = (char*) malloc(len);
        if (sysctl(mib, 6, macbuf, (size_t*)&len, NULL, 0) < 0)
        {
            return NULL;
        }
        
        struct if_msghdr * ifm = (struct if_msghdr *)macbuf;
        struct sockaddr_dl * sdl = (struct sockaddr_dl *)(ifm + 1);
        unsigned char * ptr = (unsigned char *)LLADDR(sdl);
        bcopy(ptr, res->mac[res->size++], 6);
        free(macbuf);
    }

    close(s);

    return res;
}

#else

// Linux.

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/if.h>

int getNIFNames(int socket, int maxcount, char** names) 
{
    // Kernel docs: /proc/net/dev format was stable since v2.4.0, (Nov.2000).
    FILE* netdev = fopen("/proc/net/dev", "r");
    int i;
    if (netdev) 
    { // netdev contains *all* interfaces, including inactive ones
        int MAXNAMELEN = 32; // maximum length of interface name
        char buf[MAXNAMELEN];
        int c = 0, bi;
        // skip two header lines:
        while (c != EOF && c != '\n') c = fgetc(netdev);
        if (c != EOF) c = fgetc(netdev); // read ahead
        while (c != EOF && c != '\n') c = fgetc(netdev);
        i = 0;
        while (c != EOF && i < maxcount) 
        { // read the name of interface
            // skip white space:
            while (c != EOF && isspace(c)) c = fgetc(netdev);
            buf[bi = 0] = c; // save first non-white-space character
            // read until colon (or white-space if any):
            while (c != EOF && c != ':' && !isspace(c) && bi < MAXNAMELEN) 
            {
                buf[++bi] = c = fgetc(netdev);
            }
            buf[bi] = '\0'; // erase last char (colon) with termination
            if (bi == 0) break; // smth is wrong with format, get out
            // save the name:
            strcpy(names[i++] = malloc(strlen(buf)+1), buf);
            // skip the rest of line:
            while (c != EOF && c != '\n') c = fgetc(netdev);
            if (c != EOF) c = fgetc(netdev); // peek into next char
        }
        fclose(netdev);
        if (i > 0) return i; // must have been success
    } 
    // otherwise fallback to ioctl API, which lists only active interfaces:
    struct ifreq *IFR;
    struct ifconf ifc;
    char buf[sizeof(struct ifreq) * maxcount];
    int len;
    ifc.ifc_len = sizeof(struct ifreq) * maxcount;
    ifc.ifc_buf = buf;
    if (ioctl(socket, SIOCGIFCONF, &ifc) < 0)
    {
        return 0;
    }
    IFR = ifc.ifc_req;
    len = ifc.ifc_len / sizeof(struct ifreq);
    for (i = 0; i < len; ++i) {
        strcpy(names[i] = malloc(strlen(IFR[i].ifr_name)+1), IFR[i].ifr_name);
    }
    return len;
}

maclist_t* base_getMAC()
{
    int MAXIFS = 32;
    struct ifreq ifr;
    int s, i, len;
    maclist_t *res;
    
    char* names[MAXIFS];
    bzero(names, MAXIFS*sizeof(char*));

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
    {
        return NULL;
    }

    len = getNIFNames(s, MAXIFS, names);
    if (len == 0) return NULL;

    res = (maclist_t*) malloc(sizeof(maclist_t) +
                              len * sizeof(mac_adr_t));
    if (!res)
    {
        return NULL;
    }
    res->size = 0;
    for (i = 0; i < len && names[i]; ++i)
    {
        strcpy(ifr.ifr_name, names[i]);
        if (ioctl(s, SIOCGIFFLAGS, &ifr) == 0 &&
            !(ifr.ifr_flags & IFF_LOOPBACK) &&
            (ioctl(s, SIOCGIFHWADDR, &ifr) == 0))
        {
            bcopy(ifr.ifr_hwaddr.sa_data, res->mac[res->size++], 8);
        }
    }
    close(s);
    for (i = 0; i < MAXIFS && names[i] ; ++i) free(names[i]);
        
    return res;
}

#endif
