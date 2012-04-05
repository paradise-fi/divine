#include <divine/sysinfo.h>

#include <time.h>
#include <wibble/regexp.h>
#include <fstream>
#include <stdint.h>

#ifdef POSIX
#include <sys/resource.h>
#include <sys/time.h>
#endif

#ifdef _WIN32
#include <psapi.h>
#include <setupapi.h>

#define CPU_REG_KEY    HKEY_LOCAL_MACHINE
#define CPU_REG_SUBKEY "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0"
#define CPU_SPEED      "~MHz"
#define CPU_NAME       "ProcessorNameString"
#endif

namespace divine {
namespace sysinfo {

Data *Info::data = 0;

struct Data {
#ifdef POSIX
    struct timeval tv, now;
    struct rusage usage;
#endif

#ifdef _WIN32
    HANDLE hProcess;
    FILETIME ftCreation, ftExit, ftKernel, ftUser;
    SYSTEMTIME stStart, stFinish, stKernel, stUser;
    unsigned __int64 dwSpeed;
#endif
};

static bool matchLine( std::string file, wibble::ERegexp &r ) {
    std::string line;
    std::ifstream f( file.c_str() );
    while ( !f.eof() ) {
        std::getline( f, line );
        if ( r.match( line ) )
            return true;
    }
    return false;
}

#ifdef POSIX
double interval( struct timeval from, struct timeval to ) {
    return to.tv_sec - from.tv_sec +
        double(to.tv_usec - from.tv_usec) / 1000000;
}

struct timeval zeroTime() {
    struct timeval r = { 0, 0 };
    return r;
}
#endif

#ifdef _WIN32
double SystemTimeToDouble(SYSTEMTIME &time)
{
    return (double)(time.wHour * 3600 + time.wMinute * 60 +
                    time.wSecond + (double)(time.wMilliseconds) / 1000);
}
#endif

double Info::userTime() {
#ifdef POSIX
    return interval( zeroTime(), data->usage.ru_utime );
#elif defined(_WIN32)
    return SystemTimeToDouble(data->stUser);
#endif
}

double Info::systemTime() {
#ifdef POSIX
    return interval( zeroTime(), data->usage.ru_stime );
#elif defined(_WIN32)
    return SystemTimeToDouble(data->stKernel);
#endif
}

double Info::wallTime() {
#ifdef POSIX
    return interval( data->tv, data->now );
#elif defined(_WIN32)
    return SystemTimeToDouble(data->stFinish) - SystemTimeToDouble(data->stStart);
#endif
}

Info::Info() {
    if (data)
        return;

    data = new Data;
    start();
}

void Info::start() {
#ifdef POSIX
    gettimeofday(&data->tv, NULL);
#endif

#ifdef _WIN32
    data->hProcess = GetCurrentProcess();
    GetLocalTime(&data->stStart);
#endif
}

void Info::update() {
#ifdef POSIX
    gettimeofday(&data->now, NULL);
    getrusage( RUSAGE_SELF, &data->usage );
#elif defined(_WIN32)
    GetLocalTime(&data->stFinish);
    GetProcessTimes(data->hProcess, &data->ftCreation, &data->ftExit,
                    &data->ftKernel, &data->ftUser);
#endif
}

void Info::stop() {
#if defined(_WIN32)
    CloseHandle(data->hProcess);
#endif
}

int Info::peakVmSize() {
    int vmsz = 0;
#ifdef __linux
    std::stringstream file;
    file << "/proc/" << uint64_t( getpid() ) << "/status";
    wibble::ERegexp r( "VmPeak:[\t ]*([0-9]+)", 2 );
    if ( matchLine( file.str(), r ) ) {
        vmsz = atoi( r[1].c_str() );
    }
#endif

#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (hProcess != NULL && GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc)))
        vmsz = pmc.PeakWorkingSetSize/(1024);
#endif
    return vmsz;
}

std::string Info::architecture() {
#ifdef __linux
    wibble::ERegexp r( "model name[\t ]*: (.+)", 2 );
    if ( matchLine( "/proc/cpuinfo", r ) )
        return r[1];
    return "Unknown";
#endif

#ifdef _WIN32
    HKEY hKey;
    DWORD dwType;
    DWORD dwSize;
    LONG ret = RegOpenKeyEx(CPU_REG_KEY, CPU_REG_SUBKEY, 0, KEY_READ, &hKey);

    // key could not be opened
    if(ret != ERROR_SUCCESS)
        return "Unknown";

    // get CPU name
    dwSize = 0;
    ret = RegQueryValueEx(hKey, CPU_NAME, NULL, &dwType, NULL, &dwSize);
    LPTSTR name;
    name = new TCHAR[dwSize];
    ret = RegQueryValueEx(hKey, CPU_NAME, NULL, &dwType, (LPBYTE)name, &dwSize);

    if(ret != ERROR_SUCCESS)
    {
	RegCloseKey(hKey);
	return "Unknown";
    }

    RegCloseKey(hKey);
    std::string result( name );
    delete[] name;
    return result;
#endif

    return "Unknown";
}

}

std::ostream &operator<<( std::ostream &o, sysinfo::Info i )
{
    i.update();
    o << "Architecture: " << i.architecture() << std::endl;
    o << "Memory-Used: " << i.peakVmSize() << std::endl;
    o << "User-Time: " << i.userTime() << std::endl;
    o << "System-Time: " << i.systemTime() << std::endl;
    o << "Wall-Time: " << i.wallTime() << std::endl;
    return o;
}

}

