/* TAGS: c */
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

int main() {
    int r;
    int server = socket( AF_UNIX, SOCK_STREAM, 0 );
    assert( server >= 0 );

    int client = socket( AF_UNIX, SOCK_STREAM, 0 );
    assert( client >= 0 );

    struct sockaddr_un Saddr, Caddr;
    socklen_t len = sizeof( Saddr );
    Saddr.sun_family = AF_UNIX;
    strcpy( Saddr.sun_path, "server" );

    r = bind( server, &Saddr, len );
    assert( r == 0 );

    r = listen( server, 5 );
    assert( r == 0 );

    r = connect( client, &Saddr, len );
    assert( r == 0 );

    int Send = accept( server, &Caddr, &len );
    assert( Send >= 0 );

    char buf[8] = {};

    r = send( client, "tral", 4, 0 );
    assert( r == 4 );

    r = send( client, "ala", 3, 0 );
    assert( r == 3 );


    r = -1;
    r = recv( Send, buf, 8, 0 );
    assert( r == 7 );
    assert( strcmp( buf, "tralala" ) == 0 );

    close( Send );
    close( client );

    r = access( "server", F_OK );
    assert( r == 0 );

    r = unlink( "server" );
    assert( r == 0 );

    r = access( "server", F_OK );
    assert( r == -1 );
    assert( errno == ENOENT );

    return 0;
}
