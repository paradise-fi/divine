// -*- C++ -*- (c) 2014 Vladimír Štill

#include <memory>
#include <vector>
#include <string>
#include <future>

#include <brick-fs.h>

#ifndef TOOLS_COMPILE_ENV_H
#define TOOLS_COMPILE_ENV_H

namespace divine {
namespace compile {

struct Env {

    Env() : librariesOnly( false ), useThreads( 1 ), keepBuildDir( false ), dontLink( false ) { }
    virtual ~Env() = default;

    struct RunQueue {
        RunQueue( const RunQueue &o ) : _env( o._env ) { ASSERT( _env != nullptr ); }
        RunQueue( RunQueue && ) = default;
        RunQueue() = delete;

        friend struct Env;

        void pushCompilation( std::string file, std::string flags ) {
            _commands.emplace_back( file, flags );
        }

        void clear() { _commands.clear(); }
        int size() const { return _commands.size(); }

        void run( int threads = -1 ) {
            if ( threads <= 0 )
                threads = _env->useThreads;

            ASSERT( _env );

            int extra = threads - 1;
            ASSERT_LEQ( 0, extra );

            _current = 0;
            std::vector< std::future< void > > thrs;
            for ( int i = 0; i < extra; ++i )
                thrs.emplace_back( std::async( std::launch::async, &RunQueue::_run, this ) );
            _run();

            for ( auto &t : thrs )
                t.get(); // get propagates exceptions

            clear();
        }

      private:
        RunQueue( Env *env ) : _env( env ) { ASSERT( env != nullptr ); }

        Env *_env;

        std::vector< std::pair< std::string, std::string > > _commands;
        std::atomic< size_t > _current;

        void _run() {
            int job;
            while ( (job = _current.fetch_add( 1 )) < size() )
                _env->compileFile( _commands[ job ].first, _commands[ job ].second );
        }
    };

    virtual void compileFile( std::string /* file */, std::string /* flags */ ) = 0;

    RunQueue getQueue() { return RunQueue( this ); }

    bool librariesOnly;
    std::string usePrecompiled;
    int useThreads;
    bool keepBuildDir;
    bool dontLink;
    std::vector< std::string > input;
    std::string output;
    std::string flags;

  private:
    std::vector< std::string > _cwdStack;
};

}
}

#endif // TOOLS_COMPILE_ENV_H
