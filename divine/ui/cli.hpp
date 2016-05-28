#pragma once

#include <divine/vm/bitcode.hpp>
#include <divine/vm/run.hpp>

#include <divine/ui/common.hpp>
#include <divine/ui/curses.hpp>
#include <brick-cmd>
#include <brick-fs>
#include <brick-llvm>

namespace divine {
namespace ui {

namespace cmd = brick::cmd;

struct Command
{
    virtual void run() = 0;
    virtual void setup() {}
};

struct WithBC : Command
{
    std::string _file;
    std::vector< std::string > _env;

    std::shared_ptr< vm::BitCode > _bc;
    void setup()
    {
        _bc = std::make_shared< vm::BitCode >( _file );
    }
};

struct Verify : WithBC
{
    void run() { NOT_IMPLEMENTED(); }
};

struct Run : WithBC
{
    void run() { vm::Run( _bc, _env ).run(); }
};

struct Draw   : WithBC
{
    void run() { NOT_IMPLEMENTED(); }
};

struct Link : Command
{
    std::vector< std::string > _files;
    std::string _output;

    Link() : _output( "a.bc" ) {}
    void run() override
    {
        brick::llvm::Linker linker;
        ::llvm::SMDiagnostic err;
        ::llvm::LLVMContext ctx;
        for ( auto f : _files )
        {
            auto mod = ::llvm::parseIRFile( f, err, ctx );
            if ( !mod )
                throw std::runtime_error( "error loading bitcode file " + f + "\n    " +
                                          err.getMessage().data() );
            std::cerr << "I: linking " << f << std::endl;
            linker.link( std::move( mod ) );
        }
        linker.prune( { "__sys_init", "main", "memmove", "memset", "memcpy", "llvm.global_ctors" },
                      brick::llvm::Prune::AllUnused /* TODO UnusedModules */ );
        brick::llvm::writeModule( linker.get(), _output );
    }
};

struct CLI : Interface
{
    bool _batch;
    std::vector< std::string > _args;

    CLI( int argc, const char **argv )
        : _batch( true )
    {
        for ( int i = 1; i < argc; ++i )
            _args.emplace_back( argv[ i ] );
    }

    auto validator()
    {
        return cmd::make_validator()->
            add( "file", []( std::string s, auto good, auto bad )
                 {
                     if ( s[0] == '-' ) /* FIXME! */
                         return bad( cmd::BadFormat, "file must not start with -" );
                     if ( !brick::fs::access( s, F_OK ) )
                         return bad( cmd::BadContent, "file " + s + " does not exist");
                     if ( !brick::fs::access( s, R_OK ) )
                         return bad( cmd::BadContent, "file " + s + " is not readable");
                     return good( s );
                 } );
    }

    auto parse()
    {
        auto v = validator();
        auto linkopts = cmd::make_option_set< Link >( v )
                        .add( "[-o {string}]", &Link::_output, std::string( "the output file" ) )
                        .add( "{file}+", &Link::_files, std::string( "files to link" ) );
        auto bcopts = cmd::make_option_set< WithBC >( v )
                      .add( "[-D {string}]", &WithBC::_env, std::string( "add to the environment" ) )
                      .add( "{file}", &WithBC::_file, std::string( "the bitcode file to load" ) );
        auto p = cmd::make_parser( v )
                 .add< Verify >( bcopts )
                 .add< Run >( bcopts )
                 .add< Draw >( bcopts )
                 .add< Link >( linkopts );
        return p.parse( _args.begin(), _args.end() );
    }

    std::shared_ptr< Interface > resolve() override
    {
        if ( _batch )
            return Interface::resolve();
        else
            return std::make_shared< ui::Curses >( /* ... */ );
    }

    virtual int main() override
    {
        auto cmd = parse();
        cmd.apply( [&]( Command &c )
                   {
                       c.setup();
                       c.run();
                   } );
        return 0;
    }
};

}
}
