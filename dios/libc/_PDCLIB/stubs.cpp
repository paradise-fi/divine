#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <stdarg.h>
#include <string.h>
#include <wchar.h>
#include <netdb.h>
#include <pwd.h>
#include <unistd.h>
#include <grp.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <netinet/in.h>

#include <sys/divm.h>
#include <sys/fault.h>
#include <dios.h>
#include <_PDCLIB/locale.h>
#include <_PDCLIB/cdefs.h>

#include <sys/time.h>
#include <time.h>

extern "C" {

#define NOT_IMPLEMENTED { __dios_fault( _VM_F_NotImplemented, "libc stubs" ); return 0; }

double atof( const char * ) noexcept NOT_IMPLEMENTED;
double strtod( const char *, char ** ) noexcept NOT_IMPLEMENTED;
float strtof( const char *, char ** ) noexcept NOT_IMPLEMENTED;
long double strtold( const char *, char ** ) noexcept NOT_IMPLEMENTED;
size_t mbsrtowcs( wchar_t *, const char **, size_t, mbstate_t * ) NOT_IMPLEMENTED;
wint_t btowc( int ) NOT_IMPLEMENTED;
int wctob( wint_t ) NOT_IMPLEMENTED;
size_t wcsnrtombs( char *, const wchar_t **, size_t, size_t, mbstate_t * ) NOT_IMPLEMENTED;
size_t mbsnrtowcs( wchar_t *, const char **, size_t, size_t, mbstate_t * ) NOT_IMPLEMENTED;

int wctomb( char *, wchar_t ) NOT_IMPLEMENTED;
size_t mbrlen( const char *, size_t, mbstate_t * ) NOT_IMPLEMENTED;
wchar_t *wmemset( wchar_t *, wchar_t, size_t ) NOT_IMPLEMENTED;
long wcstol( const wchar_t *, wchar_t **, int ) NOT_IMPLEMENTED;
long long wcstoll( const wchar_t *, wchar_t **, int ) NOT_IMPLEMENTED;
unsigned long wcstoul( const wchar_t *, wchar_t **, int ) NOT_IMPLEMENTED;
unsigned long long wcstoull( const wchar_t *, wchar_t **, int ) NOT_IMPLEMENTED;
double wcstod( const wchar_t *, wchar_t ** ) NOT_IMPLEMENTED;
float wcstof( const wchar_t *, wchar_t ** ) NOT_IMPLEMENTED;
long double wcstold( const wchar_t *, wchar_t ** ) NOT_IMPLEMENTED;

int wprintf( const wchar_t *, ... ) NOT_IMPLEMENTED;
int fwprintf( FILE *, const wchar_t *, ... ) NOT_IMPLEMENTED;
int swprintf( wchar_t *, size_t, const wchar_t *, ... ) NOT_IMPLEMENTED;

int vwprintf( const wchar_t *, va_list ) NOT_IMPLEMENTED;
int vfwprintf( FILE *, const wchar_t *, va_list ) NOT_IMPLEMENTED;
int vswprintf( wchar_t *, size_t, const wchar_t *, va_list ) NOT_IMPLEMENTED;

int settimeofday(const struct timeval *, const struct timezone *) NOT_IMPLEMENTED;

int getgroups(int gidsetsize, gid_t grouplist[]) NOT_IMPLEMENTED;
struct hostent* gethostbyname(const char *) NOT_IMPLEMENTED;
char *inet_ntop(int, const void *, char *, socklen_t) NOT_IMPLEMENTED;
struct passwd* getpwnam(const char *) NOT_IMPLEMENTED;
uid_t geteuid( void ) NOT_IMPLEMENTED;
gid_t getegid( void ) NOT_IMPLEMENTED;
gid_t getgid( void ) NOT_IMPLEMENTED;
uid_t getuid( void ) NOT_IMPLEMENTED;
int	ioctl(int, unsigned long, ...) NOT_IMPLEMENTED;
struct group *getgrnam(const char *name) NOT_IMPLEMENTED;
struct group *getgrgid(gid_t gid) NOT_IMPLEMENTED;
struct passwd* getpwuid(uid_t) NOT_IMPLEMENTED;
int fchown( int fildes, uid_t owner, gid_t group ) NOT_IMPLEMENTED;

void utime( const char *path, const struct utimbuf *times ) { __dios_fault( _VM_F_NotImplemented, "utime" ); };
int issetugid( void ) NOT_IMPLEMENTED;

void *dlsym( void *, void * ) NOT_IMPLEMENTED;

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) NOT_IMPLEMENTED;

int mbtowc( wchar_t *, const char *s, size_t )
{
    if ( s )/* not stateless */
        __dios_fault( _VM_F_NotImplemented, "mbtowc" );
    return 0;
}

int chown(const char* /*path*/, uid_t /*owner*/, gid_t /*group*/) NOT_IMPLEMENTED;


locale_t newlocale( int, const char *lc, locale_t )
{
    if ( strcmp( lc, "C" ) == 0 )
        return const_cast< locale_t >( &_PDCLIB_global_locale );

    __dios_fault( _VM_F_NotImplemented, "newlocale" );
    return 0;
}

int gettimeofday( struct timeval *tp, void * /* tzp */ )
{
    tp->tv_sec = 0;
    tp->tv_usec = 0;
    return 0;
}

}
