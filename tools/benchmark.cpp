// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2017 Petr Ročkai <code@fixp.eu>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <tools/benchmark.hpp>

#include <divine/ui/sysinfo.hpp>
#include <divine/ui/log.hpp>
#include <divine/ui/odbc.hpp>
#include <divine/ui/cli.hpp>

#include <iostream>
#include <vector>
#include <brick-string>
#include <brick-types>
#include <brick-cmd>

namespace benchmark
{

int Import::modrev()
{
    std::stringstream rev_q;
    rev_q << "select max(revision) from model group by name, variant having name = ? and variant = ?";
    nanodbc::statement rev( _conn, rev_q.str() );

    rev.bind( 0, _name.c_str() );
    if ( _variant.empty() )
        rev.bind_null( 1 );
    else
        rev.bind( 1, _variant.c_str() );

    int next = 1;
    try
    {
        auto r = nanodbc::execute( rev );
        r.first();
        next = 1 + r.get< int >( 0 );
    } catch (...) {}

    return next;
}

bool Import::files()
{
    if ( _name.empty() )
        _name = _files[0];

    auto str = fs::readFile( _script );
    std::vector< uint8_t > data( str.begin(), str.end() );
    int script_id = odbc::unique_id( _conn, "source", odbc::Keys{ "text" },
                                     odbc::Vals{ data } );

    std::set< std::pair< std::string, int > > file_ids;

    for ( auto file : _files )
    {
        auto src = fs::readFile( file );
        std::vector< uint8_t > data( src.begin(), src.end() );
        int file_id = odbc::unique_id(
                _conn, "source", odbc::Keys{ "text" },
                                 odbc::Vals{ data } );
        file_ids.emplace( file, file_id );
    }

    auto next_rev = modrev();

    for ( int rev = 1; rev < next_rev; ++rev )
    {
        nanodbc::statement get_script( _conn,
            "select script from model where name = ? and revision = ? and variant = ?" );
        get_script.bind( 0, _name.c_str() );
        get_script.bind( 1, &rev );
        if ( _variant.empty() )
            get_script.bind_null( 2 );
        else
            get_script.bind( 2, _variant.c_str() );
        auto scr_id = get_script.execute();
        if ( !scr_id.first() )
            continue; /* does not exist */
        if ( scr_id.get< int >( 0 ) != script_id )
            continue; /* no match */

        nanodbc::statement get_files( _conn,
            "select model_srcs.filename, model_srcs.source from "
            "model join model_srcs on model_srcs.model = model.id "
            "where model.revision = ? and model.name = ? and model.variant = ?" );
        get_files.bind( 0, &rev );
        get_files.bind( 1, _name.c_str() );
        if ( _variant.empty() )
            get_files.bind_null( 2 );
        else
            get_files.bind( 2, _variant.c_str() );

        auto file = get_files.execute();
        int match = 0, indb = 0;
        while ( file.next() )
        {
            auto k = std::make_pair( file.get< std::string >( 0 ), file.get< int >( 1 ) );
            if ( file_ids.count( k ) )
                ++ match;
            ++ indb;
        }
        if ( match == indb && match == int( file_ids.size() ) )
        {
            std::cerr << "W: not imported, identical model present as revision " << rev << std::endl;
            return false;
        }
    }

    odbc::Keys keys_mod{ "name", "revision", "script" };
    odbc::Vals vals_mod{ _name, next_rev, script_id };
    if ( !_variant.empty() )
        keys_mod.push_back( "variant" ), vals_mod.push_back( _variant );
    _id = odbc::unique_id( _conn, "model", keys_mod, vals_mod );

    for ( auto p : file_ids )
    {
        odbc::Keys keys_tie{ "model", "filename", "source" };
        odbc::Vals vals_tie{ _id, p.first, p.second };
        auto ins = odbc::insert( _conn, "model_srcs", keys_tie, vals_tie );
        nanodbc::execute( ins );
    };

    return true;
}

void Import::tag()
{
    for ( auto tag : _tags )
    {
        int tag_id = odbc::unique_id( _conn, "tag", odbc::Keys{ "name" }, odbc::Vals{ tag } );
        odbc::Vals vals{ _id, tag_id };
        auto ins = odbc::insert( _conn, "model_tags", odbc::Keys{ "model", "tag" }, vals );
        ins.execute();
    }
}

void Schedule::run()
{
    int inst = odbc::get_instance( _conn );
    std::stringstream q;
    /* XXX check that highest id is always the latest revision? */
    q << "select max( model.id ), model.name from model "
      << "join model_tags on model.id = model_tags.model "
      << "join tag on tag.id = model_tags.tag ";
    q << "group by model.name, model.variant ";
    if ( !_tag.empty() )
        q << " having tag.name = ? ";

    std::cerr << q.str() << std::endl;
    nanodbc::statement find( _conn, q.str() );
    if ( !_tag.empty() )
        find.bind( 0, _tag.c_str() );

    auto mod = find.execute();
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

void Cmd::setup()
{
    try
    {
        _conn.connect( _odbc );
    }
    catch ( nanodbc::database_error &err )
    {
        std::cerr << "could not connect: " << err.what() << std::endl;
    }
}

}

namespace cmd  = brick::cmd;
using namespace benchmark;

int main( int argc, const char **argv )
{
    auto validator = cmd::make_validator();
    auto args = cmd::from_argv( argc, argv );

    auto opts_db = cmd::make_option_set< Cmd >( validator )
        .option( "-d {string}", &Cmd::_odbc, "ODBC connection string" );

    auto opts_import = cmd::make_option_set< Import >( validator )
        .option( "[--name {string}]", &Import::_name, "model name (default: filename)" )
        .option( "[--variant {string}]", &Import::_variant, "model variant (default: null)" )
        .option( "[--tag {string}]", &Import::_tags, "tag(s) to assign to the model" )
        .option( "[--file {string}]", &Import::_files, "a (source) file to import" )
        .option( "[--script {string}]", &Import::_script, "a build/verify script" );

    auto opts_report_base = cmd::make_option_set< ReportBase >( validator )
        .option( "[--by-tag]",  &ReportBase::_by_tag, "group results by tags" )
        .option( "[--watch]",  &ReportBase::_watch, "refresh the results in a loop" )
        .option( "[--result {string}]", &ReportBase::_result,
                 "only include runs with one of given results (default: VE)" );

    auto opts_report = cmd::make_option_set< Report >( validator )
        .option( "[--list-instances]",  &Report::_list_instances, "show available instances" )
        .option( "[--instance {int}]",  &Report::_instance, "show results for a given instance" );

    auto opts_compare = cmd::make_option_set< Compare >( validator )
        .option( "[--instance {int}]",  &Compare::_instances, "compare given instances" );

    auto opts_schedule = cmd::make_option_set< Schedule >( validator )
        .option( "[--tag {string}]", &Schedule::_tag, "only schedule models with a given tag" );

    auto cmds = cmd::make_parser( cmd::make_validator() )
        .command< Import >( opts_db, opts_import )
        .command< Schedule >( opts_db, opts_schedule )
        .command< Report >( opts_db, opts_report_base, opts_report )
        .command< Compare >( opts_db, opts_report_base, opts_compare )
        .command< Run >( opts_db );
    auto cmd = cmds.parse( args.begin(), args.end() );

    cmd.match( [&]( Cmd &cmd ) { cmd.setup(); cmd.run(); } );
    return 0;
}
