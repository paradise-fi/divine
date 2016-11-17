#include <divine/ui/sysinfo.hpp>

#include <time.h>
#include <fstream>
#include <stdint.h>
#include <mutex>
#include <thread>
#include <chrono>
#include <regex>

#include <brick-types>
#include <brick-string>

#ifdef __unix
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#include <windows.h>
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
#ifdef __unix
    struct timeval tv, now;
    struct rusage usage;
#endif

#ifdef _WIN32
    HANDLE hProcess;
    FILETIME ftCreation, ftExit, ftKernel, ftUser;
    SYSTEMTIME stStart, stFinish, stKernel, stUser;
    unsigned __int64 dwSpeed;
#endif
    std::mutex lock;
};

using MaybeMatch = brick::types::Maybe< std::smatch >;

static MaybeMatch matchLine( std::string file, std::regex &r ) {
    std::string line;
    std::smatch match;
    std::ifstream f( file.c_str() );
    while ( !f.eof() ) {
        std::getline( f, line );
        if ( std::regex_match( line, match, r ) )
            return MaybeMatch::Just( match );
    }
    return MaybeMatch::Nothing();
}

#ifdef __linux
long long procStatusLine( std::string key ) {
    std::stringstream file;
    file << "/proc/" << uint64_t( getpid() ) << "/status";
    std::regex r( key + ":[\t ]*([0-9]+)", std::regex::extended );
    auto m = matchLine( file.str(), r );
    if ( m.isJust() ) {
        return std::stoll( m.value()[1] );
    }
    return 0;
}
#endif

#ifdef __unix
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

using guard = std::lock_guard< std::mutex >;

double Info::userTime() const {
    guard _l( data->lock );
#ifdef __unix
    return interval( zeroTime(), data->usage.ru_utime );
#elif defined(_WIN32)
    return SystemTimeToDouble(data->stUser);
#endif
}

double Info::systemTime() const {
    guard _l( data->lock );
#ifdef __unix
    return interval( zeroTime(), data->usage.ru_stime );
#elif defined(_WIN32)
    return SystemTimeToDouble(data->stKernel);
#endif
}

double Info::wallTime() const {
    guard _l( data->lock );
#ifdef __unix
    return interval( data->tv, data->now );
#elif defined(_WIN32)
    return SystemTimeToDouble(data->stFinish) - SystemTimeToDouble(data->stStart);
#endif
}

void Info::init() {
    if ( data )
        return;

    data = new Data;
    start();
}

Info::Info() {
    ASSERT( data );
}

void Info::start() {
    guard _l( data->lock );
#ifdef __unix
    gettimeofday(&data->tv, NULL);
#endif

#ifdef _WIN32
    data->hProcess = GetCurrentProcess();
    GetLocalTime(&data->stStart);
#endif
}

void Info::update() const {
    guard _l( data->lock );
#ifdef __unix
    gettimeofday(&data->now, NULL);
    getrusage( RUSAGE_SELF, &data->usage );
#elif defined(_WIN32)
    GetLocalTime(&data->stFinish);
    GetProcessTimes(data->hProcess, &data->ftCreation, &data->ftExit,
                    &data->ftKernel, &data->ftUser);
#endif
}

void Info::stop() const {
    guard _l( data->lock );
#if defined(_WIN32)
    CloseHandle(data->hProcess);
#endif
}

#if defined( __linux )
#define MEMINFO( fn, liKey, winKey ) uint64_t Info::fn() const { \
    return procStatusLine( liKey ); \
}
#elif defined( _WIN32 )
#define MEMINFO( fn, liKey, winKye ) uint64_t Info::fn() const { \
    guard _l( data->lock ); /* functions might not be theread safe */ \
    PROCESS_MEMORY_COUNTERS pmc; \
    if (data->hProcess != NULL && GetProcessMemoryInfo(data->hProcess, &pmc, sizeof(pmc))) \
        return pmc.PeakWorkingSetSize/(1024); \
    return 0; \
}
#else
#define MEMINFO( fn, liKey, winKey ) uint64_t Info::fn() const { return 0; }
#endif

MEMINFO( peakVmSize, "VmPeak", PeakWorkingSetSize );
MEMINFO( vmSize, "VmSize", WorkingSetSize );
MEMINFO( peakResidentMemSize, "VmHWM", QuotaPeakPagedPoolUsage ); // not sure if win versions
MEMINFO( residentMemSize, "VmRSS", QuotaPagedPoolUsage ); // are right for these two

#undef MEMINFO

std::string Info::architecture() const {
    guard _l( data->lock );
#ifdef __linux
    std::regex r( "model name[\t ]*: (.+)", std::regex::extended );
    auto m = matchLine( "/proc/cpuinfo", r );
    if ( m.isJust() )
        return m.value()[1];
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

ResourceGuard::ResourceGuard()
    : memory( 0 ), time( 0 )
{}

void ResourceGuard::loop()
{
    info.update();

    if ( memory && info.peakVmSize() > memory )
        throw ResourceLimit( "Memory limit exceeded: used "
                             + brick::string::fmt( info.peakVmSize() ) + "K / "
                             + brick::string::fmt( memory ) + "K." );
    if ( time && info.wallTime() > time )
        throw ResourceLimit( "Time limit exceeded: used "
                             + brick::string::fmt( info.wallTime() ) + "s / "
                             + brick::string::fmt( time ) + "s." );

    std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
}

std::vector< ReportLine > Info::report() const {
    update();
    return { { "Architecture", architecture() },
             { "Memory-Used", std::to_string( peakVmSize() ) },
             { "Physical-Memory-Used", std::to_string( peakResidentMemSize() ) },
             { "User-Time", std::to_string( userTime() ) },
             { "System-Time", std::to_string( systemTime() ) },
             { "Wall-Time", std::to_string( wallTime() ) }
           };
}

}
}

