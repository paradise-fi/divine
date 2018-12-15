use POSIX;
my $cc = $ARGV[0];
my $preproc = `echo '#include <sys/syscall.h>' | $cc -E -Wp,-dM -`;
my @sc;

for ( split /\n/, $preproc )
{
    push @sc, $1 if /^#define SYS_([a-zA-Z_0-9]*)/;
}

my $prog =<<'EOF';
#define _POSIX_C_SOURCE 200809
#define _DEFAULT_SOURCE 1

#include <stdio.h>
#include <assert.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <signal.h>

int offset = 0;
int padding = 0;

void output( const char *name, int size, int istime )
{
    if ( padding )
        printf( "    __uint%d_t __padding%d;\n", padding * 8, offset );
    if ( istime )
        printf( "    struct timespec %s;\n", name );
    else
        printf( "    __uint%d_t %s;\n", size * 8, name );
    offset += size;
    padding = 0;
}

void print_stat()
{
    struct stat st;
    printf("struct _HOST_stat\n{\n");

#define FIELD(n, t)                              \
    if ( (char *)&st.n - (char *)&st == offset ) \
    {                                            \
        output( #n , sizeof( st.n ), t );        \
        continue;                                \
    }

    while ( offset < sizeof( st ) )
    {

        FIELD( st_mode, 0 );
        FIELD( st_dev, 0 );
        FIELD( st_ino, 0 );
        FIELD( st_nlink, 0 );
        FIELD( st_uid, 0 );
        FIELD( st_gid, 0 );
        FIELD( st_rdev, 0 );
        FIELD( st_atim, 1 );
        FIELD( st_mtim, 1 );
        FIELD( st_ctim, 1 );
        FIELD( st_size , 0 );
        FIELD( st_blocks , 0 );
        FIELD( st_blksize, 0 );
        // FIELD( st_flags, 0 );
        // FIELD( st_gen, 0 );
        // FIELD( __st_birthtim, 1 );

        padding ++;
        offset ++;
    }

    while ( padding > 0 )
    {
        printf( "    __uint32_t __padding%d;\n", offset - padding );
        padding -= 4;
    }
    assert( padding == 0 );

    printf( "} __attribute__((packed));\n" );
}

int main()
{
EOF

sub fmt
{
    my ( $def, $val, $fmt ) = @_;
    $val = $def unless ( scalar @_ > 1 );
    $fmt = "%d" unless ( $fmt );
    $prog .= "    printf(\"#define _HOST_$def $fmt\\n\", $val);\n";
}

sub line
{
    $prog .= "    printf(\"%s\\n\", \"$_[0]\");\n";
}

sub fail
{
    print $prog;
    die $@;
}

line("#ifdef _HOST_stat");
$prog .= "    print_stat();";
line("#endif");
line("");

line("#ifndef _HOSTABI_H");
line("#define _HOSTABI_H");
line("");
fmt( "SYS_$_", "SYS_$_" ) for ( @sc );
fmt( "O_RDONLY" );
fmt( "O_WRONLY" );
fmt( "O_RDWR" );
fmt( "O_EXCL" );
fmt( "O_TRUNC" );
fmt( "O_APPEND" );
fmt( "O_NONBLOCK" );
fmt( "O_CREAT" );
fmt( "O_NOCTTY" );
fmt( "O_DIRECTORY" );
fmt( "O_NOFOLLOW" );

fmt( "SOCK_NONBLOCK" );
fmt( "SOCK_CLOEXEC" );

fmt( "MSG_PEEK" );
fmt( "MSG_DONTWAIT" );
fmt( "MSG_WAITALL" );

fmt( "S_ISUID" );
fmt( "S_ISGID" );

fmt( "S_IRWXU" );
fmt( "S_IRUSR" );
fmt( "S_IWUSR" );
fmt( "S_IXUSR" );

fmt( "S_IRWXG" );
fmt( "S_IRGRP" );
fmt( "S_IWGRP" );
fmt( "S_IXGRP" );

fmt( "S_IRWXO" );
fmt( "S_IROTH" );
fmt( "S_IWOTH" );
fmt( "S_IXOTH" );

fmt( "S_IFMT" );
fmt( "S_IFIFO" );
fmt( "S_IFCHR" );
fmt( "S_IFDIR" );
fmt( "S_IFBLK" );
fmt( "S_IFREG" );
fmt( "S_IFLNK" );
fmt( "S_IFSOCK" );
fmt( "S_ISVTX" );

fmt( "RLIMIT_CPU" );
fmt( "RLIMIT_FSIZE" );
fmt( "RLIMIT_DATA" );
fmt( "RLIMIT_STACK" );
fmt( "RLIMIT_CORE" );
fmt( "RLIMIT_RSS" );
fmt( "RLIMIT_MEMLOCK" );
fmt( "RLIMIT_NPROC" );
fmt( "RLIMIT_NOFILE" );
fmt( "RLIM_NLIMITS" );

fmt( "SIGABRT" );
fmt( "SIGFPE" );
fmt( "SIGILL" );
fmt( "SIGINT" );
fmt( "SIGSEGV" );
fmt( "SIGPIPE" );
fmt( "SIGTERM" );
fmt( "SIGQUIT" );
fmt( "SIGBUS" );
fmt( "SIGSYS" );
fmt( "SIGTRAP" );
fmt( "SIGXCPU" );
fmt( "SIGXFSZ" );
fmt( "SIGKILL" );
fmt( "SIGSTOP" );
fmt( "SIGALRM" );
fmt( "SIGHUP" );
fmt( "SIGTSTP" );
fmt( "SIGUSR1" );
fmt( "SIGUSR2" );
fmt( "SIGCHLD" );

fmt( "SIG_BLOCK" );
fmt( "SIG_UNBLOCK" );
fmt( "SIG_SETMASK" );
fmt( "_NSIG" );

my $uname = (POSIX::uname())[0];
fmt( "uname", '"' . $uname . '"', "\\\"%s\\\"" );

fmt( "is_linux",   $uname eq "Linux" ? 1 : 0);
fmt( "is_openbsd", $uname eq "OpenBSD" ? 1 : 0);
fmt( "mode_t", "(int) (8*sizeof(mode_t))", "__uint%d_t" );

$prog .= "#ifdef __GNU_LIBRARY__\n";
fmt( "is_glibc", 1 );
$prog .= "#else\n";
fmt( "is_glibc", 0 );
$prog .= "#endif\n";

line("");
line( "#endif" );

$prog .= "}\n";

print STDERR "|$cc -x c -o printabi -\n";
open CC, "|$cc -x c -o printabi -";
print CC $prog;
close CC or fail( "compile error" );
system("./printabi");
