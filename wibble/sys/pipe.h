// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <wibble/sys/macros.h>

#ifdef POSIX
#include <fcntl.h>
#include <sys/select.h>
#endif
#include <unistd.h>

#include <deque>
#include <cerrno>

#include <wibble/exception.h>
#include <wibble/sys/thread.h>
#include <wibble/sys/mutex.h>
#include <wibble/sys/exec.h>

#ifndef WIBBLE_SYS_PIPE_H
#define WIBBLE_SYS_PIPE_H

namespace wibble {
namespace sys {

namespace wexcept = wibble::exception;

struct Pipe {

    struct Writer : wibble::sys::Thread {
        int fd;
        bool close;
        std::string data;
        bool running;
        bool closed;
        wibble::sys::Mutex mutex;

        Writer() : fd( -1 ), close( false ), running( false ) {}

        void *main() {
            do {
                int wrote = 0;

                {
                    wibble::sys::MutexLock __l( mutex );
                    wrote = ::write( fd, data.c_str(), data.length() );
                    if ( wrote > 0 )
                        data.erase( data.begin(), data.begin() + wrote );
                }

                if ( wrote == -1 ) {
                    if ( blocking( errno ) )
#ifdef POSIX
                        sched_yield();
#else
                        ;
#endif
                    else
                        throw wexcept::System( "writing to pipe" );
                }
            } while ( !done() );

            wibble::sys::MutexLock __l( mutex );
            running = false;
            if ( close )
                ::close( fd );

            return 0;
        }

        bool done() {
            wibble::sys::MutexLock __l( mutex );
            if ( data.empty() )
                running = false;
            return !running;
        }

        void run( int _fd, std::string what ) {
            wibble::sys::MutexLock __l( mutex );

            if ( running )
                assert_eq( _fd, fd );
            fd = _fd;
            assert_neq( fd, -1 );

            data += what;
            if ( running )
                return;
            running = true;
            start();
        }
    };

    typedef std::deque< char > Buffer;
    Buffer buffer;
    int fd;
    bool _eof;
    Writer writer;

    Pipe( int p ) : fd( p ), _eof( false )
    {
        if ( p == -1 )
            return;
#ifdef POSIX
        if ( fcntl( fd, F_SETFL, O_NONBLOCK ) == -1 )
            throw wexcept::System( "fcntl on a pipe" );
#endif
    }
    Pipe() : fd( -1 ), _eof( false ) {}

    /* Writes data to the pipe, asynchronously. */
    void write( std::string what ) {
        writer.run( fd, what );
    }

    void close() {
        wibble::sys::MutexLock __l( writer.mutex );
        writer.close = true;
        if ( !writer.running )
            ::close( fd );
    }

    bool valid() {
        return fd != -1;
    }

    bool active() {
        return valid() && !eof();
    }

    bool eof() {
        return _eof;
    }

    static bool blocking( int err ) {
#ifdef POSIX
        return err == EAGAIN || err == EWOULDBLOCK;
#else
	return err == EAGAIN;
#endif
    }

    int readMore() {
        assert( valid() );
        char _buffer[1024];
        int r = ::read( fd, _buffer, 1023 );
        if ( r == -1 && !blocking( errno ) )
            throw wexcept::System( "reading from pipe" );
        else if ( r == -1 )
            return 0;
        if ( r == 0 )
            _eof = true;
        else
            std::copy( _buffer, _buffer + r, std::back_inserter( buffer ) );
        return r;
    }

    std::string nextChunk() {
        std::string line( buffer.begin(), buffer.end() );
        buffer.clear();
        return line;
    }

    std::string nextLine() {
        assert( valid() );
        Buffer::iterator nl =
            std::find( buffer.begin(), buffer.end(), '\n' );
        while ( nl == buffer.end() ) {
            if ( !readMore() )
                return ""; // would block, so give up
            nl = std::find( buffer.begin(), buffer.end(), '\n' );
        }
        std::string line( buffer.begin(), nl );

        if ( nl != buffer.end() )
            ++ nl;
        buffer.erase( buffer.begin(), nl );

        return line;
    }

    /* Only returns on eof() or when data is buffered. */
    void wait() {
        assert( valid() );
#ifdef POSIX
        fd_set fds;
        FD_ZERO( &fds );
#endif
        while ( buffer.empty() && !eof() ) {
            if ( readMore() )
                return;
            if ( eof() )
                return;
#ifdef POSIX
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"           
            FD_SET( fd, &fds );
            select( fd + 1, &fds, 0, 0, 0 );
#pragma GCC diagnostic pop
#else
            sleep( 1 );
#endif
        }
    }
    std::string nextLineBlocking() {
        assert( valid() );
        std::string l;
        while ( !eof() ) {
            l = nextLine();
            if ( !l.empty() )
                return l;
            if ( eof() )
                return std::string( buffer.begin(), buffer.end() );
            wait();
        }
        return l;
    }

};

struct PipeThrough
{
    std::string cmd;

    PipeThrough( const std::string& _cmd ) : cmd( _cmd ) {}

    std::string run( std::string data ) {
        int _in, _out;

#ifdef _WIN32
        Exec exec(cmd);
#elif defined POSIX
        ShellCommand exec(cmd);
#endif

        exec.setupRedirects( &_in, &_out, 0 );
        exec.fork();

        Pipe in( _in ), out( _out );

        in.write( data );
        in.close();
        std::string ret;
        while ( !out.eof() ) {
            out.wait();
            ret += out.nextChunk();
        }
        in.writer.join();
        return ret;
    }
};

}
}
#endif
