#include <mutex>
#include <iostream>

#include <fcntl.h>
#include <time.h>

#if OPT_CURSES
#include <curses.h>
#include <unistd.h>
#endif

#include <divine/utility/output.h>

struct proxycall {
    virtual void flush( std::string ) = 0;
    virtual void partial( std::string ) = 0;
};

struct proxybuf : std::stringbuf {
    std::mutex &_mutex;
    proxycall &_call;

    proxybuf( proxycall &c, std::mutex &m ) : _mutex( m ), _call( c ) {}

    int sync() {
        std::unique_lock< std::mutex > __l( _mutex );
        int r = std::stringbuf::sync();
        while ( str().find( '\n' ) != std::string::npos ) {
            std::string line( str(), 0, str().find( '\n' ) );
            _call.flush( line );
            str( std::string( str(), line.length() + 1, str().length() ) );
        }
        _call.partial( str() );
        str( "" );
        return r;
    }
};

struct StdIO : divine::Output, proxycall {
    std::mutex _mutex;

    std::ostream &_myout;
    proxybuf _progbuf;
    std::ostream _progress;
    time_t _last;

    std::string _partial;
    bool _flushed;

    void flush( std::string s ) {
        if ( _flushed )
            _myout << _partial;
        _myout << s << std::endl;
        _partial = "";
    }

    void partial( std::string s ) {
        if ( _flushed )
            _myout << _partial;
        _myout << s << std::flush;
        _partial += s;
        _flushed = false;
    }

    void repeat() {
        if ( _flushed ) {
            _myout << _partial << " <...>" << std::endl;
        } else {
            _last = ::time( 0 );
            _myout << " <...>" << std::endl;
            _flushed = true;
        }
    }

    std::ostream &statistics() {
        std::unique_lock< std::mutex > __l( _mutex );
        repeat();
        return std::cerr;
    }

    std::ostream &debug() { /* currently unused */
        static struct : std::streambuf {} buf;
        static std::ostream null( &buf );
        return null;
    }

    std::ostream &progress() {
        _last = time( nullptr );
        return _progress;
    }

    StdIO( std::ostream &o )
        : _myout( o ),
          _progbuf( *this, _mutex ),
          _progress( &_progbuf ),
          _last( 0 ), _flushed( true )
    {
    }
};

#if OPT_CURSES
struct Curses : divine::Output
{
    std::mutex _mutex;

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

    } _progcall, _statcall;

    proxybuf _progbuf, _statbuf;
    std::ostream _progstr, _statstr;

    Curses()
        : _progbuf( _progcall, _mutex ), _statbuf( _statcall, _mutex ),
          _progstr( &_progbuf ), _statstr( &_statbuf )
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
	(void)maxy;

        _progcall.win = newwin( 0, 45, 1, 0 );
        scrollok( _progcall.win, true );
        _statcall.win = newwin( 0, maxx - 45, 0, 45 );
        scrollok( _statcall.win, true );
    }

    void cleanup() {
        endwin();
    }

    ~Curses() { cleanup(); }

    void setStatsSize( int x, int y ) {
        delwin( _statcall.win );
        _statcall.win = newwin( y + 1, x + 20, 1, 45 );
        scrollok( _statcall.win, true );
    }

    std::ostream &statistics() {
        std::unique_lock< std::mutex > __l( _mutex );
        return _statstr;
    }

    std::ostream &debug() {
        static struct : std::streambuf {} buf;
        static std::ostream null( &buf );
        return null;
    }

    std::ostream &progress() {
        return _progstr;
    }

};
#endif

namespace divine {

Output *makeStdIO( std::ostream &o ) {
    return new StdIO( o );
}

Output *makeCurses() {
#if OPT_CURSES
    return new Curses();
#else
    std::cerr << "WARNING: This binary has been compiled without CURSES support." << std::endl;
    std::cerr << "Falling back to standard IO." << std::endl;
    return makeStdIO( std::cerr );
#endif
}

}

std::shared_ptr< divine::Output > divine::Output::_output( makeStdIO( std::cerr ) );

