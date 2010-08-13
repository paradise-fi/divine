#include <wibble/sys/mutex.h>
#include <iostream>

#include <fcntl.h>
#include <time.h>

#ifdef HAVE_CURSES
#include <curses.h>
#endif

#include <divine/output.h>

struct proxycall {
    virtual void flush( std::string ) = 0;
    virtual void partial( std::string ) = 0;
};

struct proxybuf : std::stringbuf {
    wibble::sys::Mutex &mutex;
    proxycall &call;

    proxybuf( proxycall &c, wibble::sys::Mutex &m) : mutex( m ), call( c ) {}

    int sync() {
        wibble::sys::MutexLock __l( mutex );
        int r = std::stringbuf::sync();
        while ( str().find( '\n' ) != std::string::npos ) {
            std::string line( str(), 0, str().find( '\n' ) );
            call.flush( line );
            str( std::string( str(), line.length() + 1, str().length() ) );
        }
        call.partial( str() );
        str( "" );
        return r;
    }
};

struct StdIO : divine::Output, proxycall {
    wibble::sys::Mutex mutex;

    std::ostream &myout;
    proxybuf m_progbuf;
    std::ostream m_progress;
    time_t last;

    std::string _partial;
    bool flushed;

    void flush( std::string s ) {
        if ( flushed )
            myout << _partial;
        myout << s << std::endl;
        _partial = "";
    }

    void partial( std::string s ) {
        if ( flushed )
            myout << _partial;
        myout << s << std::flush;
        _partial += s;
        flushed = false;
    }

    void repeat( bool print ) {
        if ( flushed ) {
            myout << _partial << " <...>" << std::endl;
        } else {
            last = ::time( 0 );
            myout << " <...>" << std::endl;
            flushed = true;
        }
    }

    std::ostream &statistics() {
        wibble::sys::MutexLock __l( mutex );
        repeat( true );
        return std::cerr;
    }

    std::ostream &debug() {
        // wibble::sys::MutexLock __l( mutex );
        static struct : std::streambuf {} buf;
        static std::ostream null(&buf);
        // repeat( last < ::time( 0 ) );
        return null;
    }

    std::ostream &progress() {
        last = time( 0 );
        return m_progress;
    }

    StdIO( std::ostream &o )
        : myout( o ),
          m_progbuf( *this, mutex ),
          m_progress( &m_progbuf ),
          last( 0 ), flushed( true )
    {
    }
};

#ifdef HAVE_CURSES
struct Curses : divine::Output
{
    wibble::sys::Mutex mutex;

    struct : proxycall {
        WINDOW *win;
        void flush( std::string s ) {
            wprintw( win, "%s\n", s.c_str() );
            wrefresh( win );
        }

        void partial( std::string s ) {
            wprintw( win, "%s", s.c_str() );
            wrefresh( win );
        }

    } progcall, statcall;

    proxybuf progbuf, statbuf;
    std::ostream progstr, statstr;

    Curses()
        : progbuf( progcall, mutex ), statbuf( statcall, mutex ),
          progstr( &progbuf ), statstr( &statbuf )
    {
        // grab the tty if available; prevents mpiexec from garbling the output
        int tty = open( "/dev/tty", O_RDWR );
        if ( tty >= 0 )
            dup2( tty, 1 );

        initscr();
        cbreak();
        // noecho();
        // nonl();
        erase();
        refresh();

        int maxy, maxx;
        getmaxyx( stdscr, maxy, maxx );

        progcall.win = newwin( 0, 45, 1, 0 );
        scrollok( progcall.win, true );
        statcall.win = newwin( 0, maxx - 45, 0, 45 );
        scrollok( statcall.win, true );
    }

    void cleanup() {
        endwin();
    }

    ~Curses() { cleanup(); }

    void setStatsSize( int x, int y ) {
        delwin( statcall.win );
        statcall.win = newwin( y + 1, x + 20, 1, 45 );
        scrollok( statcall.win, true );
    }

    std::ostream &statistics() {
        wibble::sys::MutexLock __l( mutex );
        return statstr;
    }

    std::ostream &debug() {
        static struct : std::streambuf {} buf;
        static std::ostream null(&buf);
        return null;
    }

    std::ostream &progress() {
        return progstr;
    }

};
#endif

namespace divine {

Output *makeStdIO( std::ostream &o ) {
    return new StdIO( o );
}

Output *makeCurses() {
#ifdef HAVE_CURSES
    return new Curses();
#else
    std::cerr << "WARNING: This binary has been compiled without CURSES support." << std::endl;
    std::cerr << "Falling back to standard IO." << std::endl;
    return makeStdIO( std::cerr );
#endif
}

}

StdIO defoutput( std::cerr );
divine::Output *divine::Output::_output = &defoutput;

