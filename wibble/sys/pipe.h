// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <wibble/sys/macros.h>

#ifdef POSIX
#include <fcntl.h>
#include <sys/select.h>

#include <deque>
#include <cerrno>

#include <wibble/exception.h>

#ifndef WIBBLE_SYS_PIPE_H
#define WIBBLE_SYS_PIPE_H

namespace wibble {
namespace sys {

namespace wexcept = wibble::exception;

struct Pipe {
    typedef std::deque< char > Buffer;
    Buffer buffer;
    int fd;
    bool _eof;

    Pipe( int p ) : fd( p ), _eof( false )
    {
        if ( p == -1 )
            return;
        if ( fcntl( fd, F_SETFL, O_NONBLOCK ) == -1 )
            throw wexcept::System( "fcntl on a pipe" );
    }
    Pipe() : fd( -1 ), _eof( false ) {}

    void write( std::string what ) {
        ::write( fd, what.c_str(), what.length() );
    }

    void close() {
        ::close( fd );
    }

    bool active() {
        return fd != -1 && !_eof;
    }

    bool eof() {
        return _eof;
    }

    int readMore() {
        char _buffer[1024];
        int r = ::read( fd, _buffer, 1023 );
        if ( r == -1 && errno != EAGAIN )
            throw wexcept::System( "reading from pipe" );
        else if ( r == -1 )
            return 0;
        if ( r == 0 )
            _eof = true;
        else
            std::copy( _buffer, _buffer + r, std::back_inserter( buffer ) );
        return r;
    }

    std::string nextLine() {
        Buffer::iterator nl =
            std::find( buffer.begin(), buffer.end(), '\n' );
        while ( nl == buffer.end() && readMore() );
        nl = std::find( buffer.begin(), buffer.end(), '\n' );
        if ( nl == buffer.end() )
            return "";

        std::string line( buffer.begin(), nl );
        ++ nl;
        buffer.erase( buffer.begin(), nl );
        return line;
    }

    std::string nextLineBlocking() {
        fd_set fds;
        FD_ZERO( &fds );
        std::string l;
        while ( !eof() ) {
            l = nextLine();
            if ( !l.empty() )
                return l;
            FD_SET( fd, &fds );
            select( fd + 1, &fds, 0, 0, 0 );
        }
        return l;
    }

};

}
}
#endif
#endif
