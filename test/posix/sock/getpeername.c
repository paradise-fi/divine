/* TAGS: c */
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/trace.h>

int main()
{
    int server = socket( AF_UNIX, SOCK_STREAM, 0 );
    assert( server >= 0 );

    int client = socket( AF_UNIX, SOCK_STREAM, 0 );
    assert( client >= 0 );

    struct sockaddr_un server_addr, client_addr, peer_addr;
    socklen_t len = sizeof( server_addr );
    server_addr.sun_family = AF_UNIX;
    strcpy( server_addr.sun_path, "server" );

    assert( bind( server, (struct sockaddr *) &server_addr, len ) == 0 );
    assert( listen( server, 5 ) == 0 );
    assert( connect( client, (struct sockaddr *) &server_addr, len ) == 0 );
    assert( accept( server, (struct sockaddr *) &client_addr, &len ) > 0 );

    assert( getpeername( client, (struct sockaddr *) &peer_addr, &len ) == 0 );
    __dios_trace_f( "client got address '%s', len %d", peer_addr.sun_path, len );

    /* Not all systems appear to support getpeername on AF_UNIX sockets (the
     * address comes out empty on Linux, but gives the expected 'server' on
     * OpenBSD). */
    assert( !strcmp( peer_addr.sun_path, "server" ) );

    /* NB. The below is the kind of behaviour Linux implements. It is not clear
     * from POSIX whether that is the intended behaviour -- getting sizeof(
     * struct sockaddr_un ) back would also appear to be legit and is what
     * OpenBSD does. */
    assert( len == sizeof( struct sockaddr ) + strlen( peer_addr.sun_path ) + 1 );

    return 0;
}
