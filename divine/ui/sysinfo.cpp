// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2012-2016 Petr Ročkai <code@fixp.eu>
 * (c) 2016 Vladimír Štill <xstill@fi.muni.cz>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <divine/ui/sysinfo.hpp>

#include <time.h>
#include <fstream>
#include <stdint.h>
#include <mutex>
#include <thread>
#include <chrono>
#include <regex>
#include <optional>

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
namespace ui {

struct SysInfo::Data {
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

using MaybeStr = std::optional< std::string >;

static MaybeStr matchLine( std::string_view file, std::regex &r, int matchIndex )
{
    std::string line;
    std::smatch match;
    std::ifstream f( file );
    while ( !f.eof() )
    {
        std::getline( f, line );
        if ( std::regex_match( line, match, r ) )
            return match[ matchIndex ].str();
    }
    return std::nullopt;
}

#ifdef __linux
long long procStatusLine( std::string key )
{
    std::regex r( key + ":[\t ]*([0-9]+) .*", std::regex::extended );
    auto m = matchLine( brq::format( "/proc/", getpid(), "/status" ), r, 1 );
    if ( m.has_value() )
        return std::atol( m.value().c_str() );
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

double SysInfo::userTime() const {
#ifdef __unix
    return interval( zeroTime(), _data->usage.ru_utime );
#elif defined(_WIN32)
    return SystemTimeToDouble( _data->stUser );
#endif
}

double SysInfo::systemTime() const {
#ifdef __unix
    return interval( zeroTime(), _data->usage.ru_stime );
#elif defined(_WIN32)
    return SystemTimeToDouble( _data->stKernel );
#endif
}

double SysInfo::wallTime() const {
#ifdef __unix
    return interval( _data->tv, _data->now );
#elif defined(_WIN32)
    return SystemTimeToDouble( _data->stFinish ) - SystemTimeToDouble( _data->stStart );
#endif
}

SysInfo::SysInfo() : _data( new SysInfo::Data() )
{
#ifdef __unix
    gettimeofday( &_data->tv, NULL );
#endif

#ifdef _WIN32
    _data->hProcess = GetCurrentProcess();
    GetLocalTime( &_data->stStart );
#endif
}

SysInfo::~SysInfo() {
#if defined(_WIN32)
    CloseHandle( _data->hProcess );
#endif
}

void SysInfo::update() {
#ifdef __unix
    gettimeofday( &_data->now, NULL );
    getrusage( RUSAGE_SELF, &_data->usage );
#elif defined(_WIN32)
    GetLocalTime( &_data->stFinish );
    GetProcessTimes( _data->hProcess, &_data->ftCreation, &_data->ftExit,
                     &_data->ftKernel, &_data->ftUser );
#endif
}

void SysInfo::updateAndCheckTimeLimit( uint64_t time )
{
    update();

    if ( time && wallTime() > time )
        throw TimeLimit( wallTime(), time );
}

#if defined( __linux )
#define MEMINFO( fn, liKey, winKey ) uint64_t SysInfo::fn() const { \
    return procStatusLine( liKey ); \
}
#elif defined( _WIN32 )
#define MEMINFO( fn, liKey, winKey ) uint64_t SysInfo::fn() const { \
    PROCESS_MEMORY_COUNTERS pmc; \
    if ( _data->hProcess != NULL && GetProcessMemorySysInfo( _data->hProcess, &pmc, sizeof(pmc) ) ) \
        return pmc.winKey/(1024); \
    return 0; \
}
#else
#define MEMINFO( fn, liKey, winKey ) uint64_t SysInfo::fn() const { return 0; }
#endif

MEMINFO( peakVmSize, "VmPeak", PeakWorkingSetSize );
MEMINFO( vmSize, "VmSize", WorkingSetSize );
// MEMINFO( peakResidentMemSize, "VmHWM", QuotaPeakPagedPoolUsage ); // not sure if win versions
MEMINFO( residentMemSize, "VmRSS", QuotaPagedPoolUsage ); // are right for these two

uint64_t SysInfo::peakResidentMemSize() const {
    return _data->usage.ru_maxrss;
}

#undef MEMINFO

uint64_t SysInfo::memory() const
{
#ifdef __linux
    std::regex r( "MemTotal[\t ]*: (.+)", std::regex::extended );
    auto m = matchLine( "/proc/meminfo", r, 1 );
    if ( m.has_value() )
        return std::atoll( m.value().c_str() );
#endif
    return 0;
}

std::string SysInfo::architecture() const {
#ifdef __linux
    std::regex r( "model name[\t ]*: (.+)", std::regex::extended );
    auto m = matchLine( "/proc/cpuinfo", r, 1 );
    return m.value_or( "Unknown" );
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

void SysInfo::report( std::function< void ( std::string, std::string ) > yield ) {
    update();
    yield( "architecture", architecture() );
    if ( auto pvs = peakVmSize() )
        yield( "memory used", std::to_string( pvs ) );
    if ( auto prms = peakResidentMemSize() )
        yield( "physical memory used", std::to_string( prms ) );
    yield( "user time", std::to_string( userTime() ) );
    yield( "system time", std::to_string( systemTime() ) );
    yield( "wall time", std::to_string( wallTime() ) );
}

void SysInfo::setMemoryLimitInBytes( uint64_t memory ) {
    if ( !memory )
        return;
#ifdef __unix
    struct rlimit r = { memory, memory };
#ifdef __OpenBSD__
    setrlimit( RLIMIT_DATA, &r );
#else
    if ( double pvs = peakVmSize() )
        if ( memory < pvs * 1024 * 1.1 )
            throw std::runtime_error( "memory limit lower than memory required to start" );
    setrlimit( RLIMIT_AS, &r );
#endif
#else
    throw std::runtime_error( "setMemoryLimitInBytes called on a platform which does not support it" );
#endif
}

}
}

