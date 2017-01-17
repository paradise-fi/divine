#include <divine/ui/sysinfo.hpp>
#include <divine/ui/log.hpp>
#include <divine/ui/odbc.hpp>
#include <divine/ui/cli.hpp>
#include <divine/ui/parser.hpp>
#include <divine/cc/compile.hpp>

#include <iostream>
#include <vector>
#include <brick-string>
#include <brick-types>
#include <brick-cmd>

using namespace std::literals;

using namespace nanodbc;
using namespace divine;

namespace cmd  = brick::cmd;
namespace fs   = brick::fs;
namespace odbc = divine::ui::odbc;

struct Cmd
{
    std::string _odbc;
    connection  _conn;
    void setup();
    virtual void run() = 0;
};

struct Import : Cmd
{
    std::string _pkg, _name;
    std::vector< std::string > _srcs, _hdrs;

    int modrev()
    {
        nanodbc::statement rev(
                _conn, "select max(revision) from model where name = ? group by name" );
        rev.bind( 0, _name.c_str() );

        int next = 1;
        try
        {
            auto r = nanodbc::execute( rev );
            r.first();
            next = 1 + r.get< int >( 0 );
        } catch (...) {}

        return next;
    }

    void files()
    {
        if ( _name.empty() )
            _name = _srcs[0];

        std::stringstream script;

        script << "cc -o testcase.bc ";
        for ( auto s : _srcs )
            script << s << " ";
        script << std::endl << "verify testcase.bc";

        int script_id = odbc::unique_id( _conn, "source", odbc::Keys{ "text" },
                                         odbc::Vals{ script.str() } );

        odbc::Keys keys_mod{ "name", "revision", "script" };
        odbc::Vals vals_mod{ _name, modrev(), script_id };
        int mod_id = odbc::unique_id( _conn, "model", keys_mod, vals_mod );

        auto addfile = [&]( std::string file )
        {
            auto src = fs::readFile( file );
            int file_id = odbc::unique_id(
                    _conn, "source", odbc::Keys{ "text" },
                                     odbc::Vals{ src } );

            odbc::Keys keys_tie{ "model", "source", "filename" };
            odbc::Vals vals_tie{ mod_id, file_id, fs::basename( file ) };
            auto ins = odbc::insert( _conn, "model_srcs", keys_tie, vals_tie );
            nanodbc::execute( ins );
        };

        for ( auto src : _srcs ) addfile( src );
        for ( auto hdr : _hdrs ) addfile( hdr );
    }

    void pkg() { NOT_IMPLEMENTED(); }

    virtual void run()
    {
        if ( !_srcs.empty() && !_pkg.empty() )
            throw brick::except::Error( "--src and --pkg can't be both specified" );

        if ( _pkg.empty() )
            files();
        else
            pkg();
    }
};

struct Schedule : Cmd
{
    void run() override
    {
        int inst = odbc::get_instance( _conn );
        /* XXX check that highest id is always the latest revision? */
        auto mod = nanodbc::execute( _conn, "select max( id ), name from model group by name" );
        while ( mod.next() )
        {
            nanodbc::statement ins( _conn, "insert into job ( model, instance, status ) "
                                           "values (?, ?, 'P')" );
            int mod_id = mod.get< int >( 0 );
            ins.bind( 0, &mod_id );
            ins.bind( 1, &inst );
            ins.execute();
            std::cerr << "scheduled " << mod.get< std::string >( 1 ) << std::endl;
        }
    }
};

struct Run : Cmd
{
    std::vector< std::pair< std::string, std::string > > _files;
    std::vector< std::string > _script;
    bool _done;

    void prepare( int model );

    void execute( int job );
    void execute( int, ui::Verify & );
    void execute( int, ui::Run & ) { NOT_IMPLEMENTED(); }

    virtual void run();
};

void Cmd::setup()
{
    try
    {
        _conn.connect( _odbc );
        std::cerr << "connected to " << _odbc << std::endl;
    }
    catch ( nanodbc::database_error &err )
    {
        std::cerr << "could not connect: " << err.what() << std::endl;
    }
}

void Run::prepare( int model )
{
    _files.clear();
    _script.clear();

    std::cerr << "loading model " << model << std::endl;

    auto sel = nanodbc::statement( _conn,
            "select filename, text from model join source join model_srcs "
            "on model_srcs.model = model.id and model_srcs.source = source.id "
            "where model.id = ?" );
    sel.bind( 0, &model );
    auto file = sel.execute();

    while ( file.next() )
        _files.emplace_back( file.get< std::string >( 0 ),
                             file.get< std::string >( 1 ) );

    auto scr = nanodbc::statement( _conn,
            "select text from model join source on model.script = source.id where model.id = ?" );
    scr.bind( 0, &model );
    auto script = scr.execute();
    script.first();
    std::string txt = script.get< std::string >( 0 );
    brick::string::Splitter split( "\n", std::regex::extended );
    std::copy( split.begin( txt ), split.end(), std::back_inserter( _script ) );
}

void Run::execute( int job_id )
{
    _done = false;
    brick::string::Splitter split( "[ \t\n]", std::regex::extended );

    for ( auto cmdstr : _script )
    {
        if ( _done )
            throw brick::except::Error( "only one model checker run is allowed per model" );

        std::vector< std::string > tok;
        std::copy( split.begin( cmdstr ), split.end(), std::back_inserter( tok ) );
        ui::CLI cli( tok );

        auto cmd = cli.parse( cli.commands() );
        cmd.match( [&]( ui::Cc &cc ) { cc._files = _files; } );
        cmd.match( [&]( ui::Command &c ) { c.setup(); } );
        cmd.match( [&]( ui::Verify &v ) { execute( job_id, v ); },
                   [&]( ui::Run &r ) { execute( job_id, r ); },
                   [&]( ui::Cc &cc ) { cc.run(); },
                   [&]( ui::Command & )
                   {
                       throw brick::except::Error( "unsupported command " + cmdstr );
                   } );
    }
}

void Run::execute( int job_id, ui::Verify &job )
{
    _done = true;
    job._interactive = false;
    job._report = ui::Report::None;
    job.setup();

    auto log = ui::make_odbc( _odbc );
    job._log = log;
    int exec_id = log->log_id();

    nanodbc::statement exec( _conn, "update job set execution = ? where id = ?" );
    exec.bind( 0, &exec_id );
    exec.bind( 1, &job_id );
    exec.execute();

    job.run();

    nanodbc::statement done( _conn, "update job set status = 'D' where id = ?" );
    done.bind( 0, &job_id );
    done.execute();
}

void Run::run()
{
    int inst = odbc::get_instance( _conn );
    std::cerr << "instance = " << inst << std::endl;
    auto sel = nanodbc::statement( _conn, "select id, model from job where instance = ?" );
    sel.bind( 0, &inst );
    auto job = nanodbc::execute( sel );

    while ( job.next() )
    {
        int job_id = job.get< int >( 0 );
        /* claim the job */
        nanodbc::statement claim( _conn, "update job set status = 'R'"
                                         " where id = ? and status = 'P'" );
        claim.bind( 0, &job_id );
        auto res = claim.execute();
        if ( !res || res.affected_rows() != 1 )
            continue; /* somebody beat us to this one */

        try {
            prepare( job.get< int >( 1 ) );
            execute( job_id );
        } catch ( std::exception &e )
        {
            std::cerr << "W: job " << job_id << " failed: " << e.what() << std::endl;
        }
    }
}

int main( int argc, const char **argv )
{
    auto validator = cmd::make_validator();
    auto args = cmd::from_argv( argc, argv );
    auto opts_db = cmd::make_option_set< Cmd >( validator )
        .option( "-d {string}", &Cmd::_odbc, "ODBC connection string" );

    auto opts_import = cmd::make_option_set< Import >( validator )
        .option( "[--name {string}]", &Import::_name, "model name (default: filename)" )
        .option( "[--src {string}]", &Import::_srcs, "a source file to import" )
        .option( "[--hdr {string}]", &Import::_hdrs, "a header to go along" )
        .option( "[--pkg {string}]",  &Import::_pkg,  "a multi-file bundle" );

    auto cmds = cmd::make_parser( cmd::make_validator() )
        .command< Import >( opts_db, opts_import )
        .command< Schedule >( opts_db )
        .command< Run >( opts_db );
    auto cmd = cmds.parse( args.begin(), args.end() );

    cmd.match( [&]( Cmd &cmd ) { cmd.setup(); cmd.run(); } );
    return 0;
}
