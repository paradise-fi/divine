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

    char buf[8];
    assert( send( server, "asdf", 5, 0 ) == -1 ); /* no peer */
    assert( errno == ENOTCONN );
    return 0;
}
