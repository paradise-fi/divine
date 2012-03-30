// -*- C++ -*- (c) 2007, 2008, 2009 Petr Rockai <me@mornfall.net>

#include <iomanip>
#include <sstream>

#include <wibble/regexp.h>
#include <divine/config.h>
#include <divine/version.h>

#include <signal.h>
#include <time.h>

#ifdef POSIX
#include <sys/resource.h>
#include <sys/time.h>
#endif

#ifdef O_MPI
#include <mpi.h>
#ifdef O_OMPI_VERSION
#include <ompi/version.h>
#endif
#endif

#ifdef _WIN32
#include <psapi.h>
#include <setupapi.h>

#define CPU_REG_KEY    HKEY_LOCAL_MACHINE
#define CPU_REG_SUBKEY "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0"
#define CPU_SPEED      "~MHz"
#define CPU_NAME       "ProcessorNameString"
#endif

#ifndef DIVINE_REPORT_H
#define DIVINE_REPORT_H

namespace divine {

struct Result
{
    enum R { Yes, No, Unknown };
    enum T { NoCE, Cycle, Deadlock, Goal };
    R propertyHolds, fullyExplored;
    T ceType;
    uint64_t visited, expanded, deadlocks, transitions;
    std::string iniTrail, cycleTrail;
    Result() :
        propertyHolds( Unknown ), fullyExplored( Unknown ),
        ceType( NoCE ),
        visited( 0 ), expanded( 0 ), deadlocks( 0 ), transitions( 0 ),
        iniTrail( "-" ), cycleTrail( "-" )
    {}
};

static inline std::ostream &operator<<( std::ostream &o, Result::R v )
{
    return o << (v == Result::Unknown ? "-" :
                 (v == Result::Yes ? "Yes" : "No" ) );
}

struct Report
{
    Config &config;
#ifdef POSIX
    struct timeval tv;
    struct rusage usage;
#endif

#ifdef _WIN32
    HANDLE hProcess;
    FILETIME ftCreation, ftExit, ftKernel, ftUser;
    SYSTEMTIME stStart, stFinish, stKernel, stUser;
    unsigned __int64 dwSpeed;
#endif
    Result res;

    int sig;
    bool m_finished;
    bool m_dumped;
    std::string mpi_info;
    std::string algorithm;
    std::string generator;
    std::vector< std::string > reductions;

    Report( Config &c ) : config( c ),
                          sig( 0 ),
                          m_finished( false ),
                          m_dumped( false ),
                          mpi_info( "-" )
    {
#ifdef POSIX
        gettimeofday(&tv, NULL);
#endif

#ifdef _WIN32
	hProcess = GetCurrentProcess();
        GetLocalTime(&stStart);
#endif
    }

#ifdef _WIN32
    ~Report()
    {
	CloseHandle(hProcess);
    }
#endif

    void signal( int s )
    {
        m_finished = false;
        sig = s;
    }

    void finished( Result r )
    {
        m_finished = true;
        res = r;
    }

    struct timeval zeroTime() {
        struct timeval r = { 0, 0 };
        return r;
    }

#ifdef POSIX
    double userTime() {
        return interval( zeroTime(), usage.ru_utime );
    }

    double systemTime() {
	return interval( zeroTime(), usage.ru_stime );
    }

    double interval( struct timeval from, struct timeval to ) {
        return to.tv_sec - from.tv_sec +
            double(to.tv_usec - from.tv_usec) / 1000000;
    }
#endif

#ifdef _WIN32
    double SystemTimeToDouble(SYSTEMTIME &time)
    {
      return (double)(time.wHour * 3600 + time.wMinute * 60 +
		      time.wSecond + (double)(time.wMilliseconds) / 1000);
    }
#endif

    int vmSize() {
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

    static std::string architecture() {
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

    template< typename Mpi >
    void mpiInfo( Mpi &mpi ) {
#ifdef O_MPI
        std::stringstream s;
        if (mpi.size() > 1)
            s << mpi.size() << " nodes";
        else
            s << "not used";
        mpi_info = s.str();
#endif
    }

    std::string reduction_info() {
        if ( reductions.empty() )
            return "None";
        std::stringstream str;
        for ( std::vector< std::string >::iterator i = reductions.begin();
              i != reductions.end(); ++i )
            str << *i << ", ";
        return std::string( str.str(), 0, str.str().length() - 2 );
    }

    std::string ceTypeString( Result::T t ) {
        switch (t) {
            case Result::NoCE: return "none";
            case Result::Goal: return "goal";
            case Result::Cycle: return "cycle";
            case Result::Deadlock: return "deadlock";
        }
    }

    static void about( std::ostream &o ) {
        o << "Version: " << versionString() << std::endl;
        o << "Build-Date: " << buildDateString() << std::endl;
        o << "Architecture: " << architecture() << std::endl;
        o << "Pointer-Width: " << 8 * sizeof( void* ) << std::endl;
#ifdef NDEBUG
        o << "Debug: disabled" << std::endl;
#else
        o << "Debug: enabled" << std::endl;
#endif

#ifdef O_MPI
        int vers, subvers;
        std::string impl = "unknown implementation";
        MPI::Get_version( vers, subvers );
#ifdef OMPI_VERSION
        impl = std::string( "OpenMPI " ) + OMPI_VERSION;
#endif
        o << "MPI-Version: " << vers << "." << subvers << " (" << impl << ")" << std::endl;
#else
        o << "MPI-Version: n/a" << std::endl;
#endif
    }

    void final( std::ostream &o ) {
        if ( m_dumped )
            return;
        m_dumped = true;

#ifdef POSIX
        struct timeval now;
        gettimeofday(&now, NULL);
        getrusage( RUSAGE_SELF, &usage );
#endif

#ifdef _WIN32
        GetLocalTime(&stFinish);
        GetProcessTimes(hProcess, &ftCreation, &ftExit, &ftKernel, &ftUser);
#endif
        about( o );
        o << std::endl;
        o << "Algorithm: " << algorithm << std::endl;
        o << "Generator: " << generator << std::endl;
        o << "Reductions: " << reduction_info() << std::endl;
        o << "MPI: " << mpi_info << std::endl;
        o << std::endl;
#ifdef POSIX
        o << "User-Time: " << userTime() << std::endl;
        o << "System-Time: " << systemTime() << std::endl;
        o << "Wall-Time: " << interval( tv, now ) << std::endl;
#endif

#ifdef _WIN32
	FileTimeToSystemTime(&ftUser, &stUser);
        o << "User-Time: " <<  SystemTimeToDouble(stUser) << std::endl;
	FileTimeToSystemTime(&ftKernel, &stKernel);
        o << "System-Time: " << SystemTimeToDouble(stKernel) << std::endl;
        o << "Wall-Time: " << SystemTimeToDouble(stFinish) - SystemTimeToDouble(stStart) << std::endl;
#endif
        o << "Memory-Used: " << vmSize() << std::endl;
        o << "Termination-Signal: " << sig << std::endl;
        o << std::endl;
        o << "Finished: " << (m_finished ? "Yes" : "No") << std::endl;
        o << "Property-Holds: " << res.propertyHolds << std::endl;
        o << "Full-State-Space: " << res.fullyExplored << std::endl;
        o << std::endl;
        o << "States-Visited: " << res.visited << std::endl;
        o << "States-Expanded: " << res.expanded << std::endl;
        o << "Transition-Count: " << res.transitions << std::endl;
        o << "Deadlock-Count: " << res.deadlocks << std::endl;
        o << std::endl;
        o << "CE-Type: " << ceTypeString( res.ceType ) << std::endl;
        o << "CE-Init: " << res.iniTrail << std::endl;
        o << "CE-Cycle: " << res.cycleTrail << std::endl;
    }
};

}

#endif
