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
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/uio.h>
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
size_t mbstowcs( wchar_t *, const char *, size_t ) NOT_IMPLEMENTED;
size_t wcstombs( char *, const wchar_t *, size_t ) NOT_IMPLEMENTED;
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

int getgroups(int gidsetsize, gid_t grouplist[]) NOT_IMPLEMENTED;
struct hostent* gethostbyname(const char *) NOT_IMPLEMENTED;
const char *inet_ntop(int, const void *, char *, socklen_t) NOT_IMPLEMENTED;
int inet_pton(int, const char *src, void *dst) NOT_IMPLEMENTED;

struct passwd* getpwnam(const char *) NOT_IMPLEMENTED;
uid_t geteuid( void ) NOT_IMPLEMENTED;
gid_t getegid( void ) NOT_IMPLEMENTED;
gid_t getgid( void ) NOT_IMPLEMENTED;
uid_t getuid( void ) NOT_IMPLEMENTED;
int	ioctl(int, unsigned long, ...) NOT_IMPLEMENTED;

struct group *getgrnam(const char *name) NOT_IMPLEMENTED;
struct group *getgrent( void ) NOT_IMPLEMENTED;
struct group *getgrgid(gid_t gid) NOT_IMPLEMENTED;
struct passwd* getpwuid(uid_t) NOT_IMPLEMENTED;
void setgrent( void ) { __dios_fault( _VM_F_NotImplemented, "setgrent" ); }
void endgrent( void ) { __dios_fault( _VM_F_NotImplemented, "endgrent" ); }

int fchown( int fildes, uid_t owner, gid_t group ) NOT_IMPLEMENTED;

clock_t times( struct tms * ) NOT_IMPLEMENTED;
void utime( const char *path, const struct utimbuf *times ) { __dios_fault( _VM_F_NotImplemented, "utime" ); };
int utimes( const char *filename, const struct timeval times[2] ) { __dios_fault( _VM_F_NotImplemented, "utimes" ); };

void *dlsym( void *, void * ) NOT_IMPLEMENTED;

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) NOT_IMPLEMENTED;
int pthread_sigmask(int how, const sigset_t *set, sigset_t *oldset) NOT_IMPLEMENTED;

int mbtowc( wchar_t *, const char *s, size_t )
{
    if ( s )/* not stateless */
        __dios_fault( _VM_F_NotImplemented, "mbtowc" );
    return 0;
}

int chown(const char* /*path*/, uid_t /*owner*/, gid_t /*group*/) NOT_IMPLEMENTED;

FILE *setmntent (const char *__file, const char *__mode) NOT_IMPLEMENTED;
struct mntent *getmntent (FILE *__stream) NOT_IMPLEMENTED;
int addmntent (FILE *__restrict __stream, const struct mntent *__restrict __mnt) NOT_IMPLEMENTED;
int endmntent (FILE *__stream) NOT_IMPLEMENTED;
char *hasmntopt (const struct mntent *__mnt, const char *__opt) NOT_IMPLEMENTED;

size_t __fbufsize (FILE *__fp) NOT_IMPLEMENTED;
int __freading (FILE *__fp) NOT_IMPLEMENTED;
int __fwriting (FILE *__fp) NOT_IMPLEMENTED;
int __freadable (FILE *__fp) NOT_IMPLEMENTED;
int __fwritable (FILE *__fp) NOT_IMPLEMENTED;
int __flbf (FILE *__fp) NOT_IMPLEMENTED;
void __fpurge (FILE *__fp) { __dios_fault( _VM_F_NotImplemented, "__fpurge" ); }
int __fsetlocking (FILE *__fp, int __type) NOT_IMPLEMENTED;

size_t __freadahead(FILE *) NOT_IMPLEMENTED;
const char *__freadptr(FILE *, size_t *) NOT_IMPLEMENTED;
void __freadptrinc(FILE *, size_t) { __dios_fault( _VM_F_NotImplemented, "__freadptrinc" ); }
void __fseterr(FILE *) { __dios_fault( _VM_F_NotImplemented, "__fseterr" ); }

int execvp( const char *file, char *const argv[] ) NOT_IMPLEMENTED;
int execlp( const char *file, const char *arg, ... ) NOT_IMPLEMENTED;
int execl( const char *file, const char *arg, ... ) NOT_IMPLEMENTED;

int select(int, fd_set *, fd_set *, fd_set *, struct timeval *timeout) NOT_IMPLEMENTED;

long sysconf( int ) NOT_IMPLEMENTED;
int nice( int ) NOT_IMPLEMENTED;
int gethostname( char *, size_t ) NOT_IMPLEMENTED;
int getaddrinfo(const char *, const char *, const struct addrinfo *, struct addrinfo **) NOT_IMPLEMENTED;
void freeaddrinfo(struct addrinfo *) { __dios_fault( _VM_F_NotImplemented, "freeaddrinfo" ); }
void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset) NOT_IMPLEMENTED;
int munmap(void *addr, size_t len) NOT_IMPLEMENTED;
int munlockall() NOT_IMPLEMENTED;
int mlockall( int ) NOT_IMPLEMENTED;
unsigned alarm( unsigned ) NOT_IMPLEMENTED;
char *getlogin() NOT_IMPLEMENTED;

long fpathconf( int, int ) NOT_IMPLEMENTED;
long pathconf( const char *, int ) NOT_IMPLEMENTED;

FILE *popen( const char *command, const char *type ) { __dios_fault( _VM_F_NotImplemented, "popen" ); }
int pclose( FILE* stream ) { __dios_fault( _VM_F_NotImplemented, "pclose" ); }

const char *gai_strerror(int) NOT_IMPLEMENTED;
int getrusage( int who, struct rusage *r_usage ) { __dios_fault( _VM_F_NotImplemented, "getrusage" ); }
extern int getsockopt( int fd, int level, int optname, void *optval, socklen_t *optlen ) NOT_IMPLEMENTED;
extern int setsockopt( int fd, int level, int optname, const void *optval, socklen_t optlen ) NOT_IMPLEMENTED;

ssize_t writev(int fildes, const struct iovec *iov, int iovcnt) NOT_IMPLEMENTED;
ssize_t readv(int fildes, const struct iovec *iov, int iovcnt) NOT_IMPLEMENTED;

}
