// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2017,2018 Petr Ročkai <code@fixp.eu>
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

#include <tools/divbench.hpp>

#include <divine/ui/sysinfo.hpp>
#include <divine/ui/log.hpp>
#include <divine/ui/odbc.hpp>
#include <divine/ui/cli.hpp>

#include <iostream>
#include <vector>
#include <brick-string>
#include <brick-types>
#include <brick-yaml>
#include <brick-proc>
#include <brick-sha2>

namespace benchmark
{

int get_instance( connection conn, int config, int build )
{
    int mach = odbc::get_machine( conn );
    if ( !build ) build = odbc::get_build( conn );
    odbc::Keys keys{ "build", "machine", "config" };
    odbc::Vals vals{ build, mach, config };
    return odbc::unique_id( conn, "instance", keys, vals );
}

void WithConnection::add_tag( std::string table, int id, std::string tag )
{
    int tag_id = odbc::unique_id( _conn, "tag", odbc::Keys{ "name" }, odbc::Vals{ tag } );
    odbc::Vals vals{ id, tag_id };
    auto ins = odbc::insert( _conn, table + "_tags", odbc::Keys{ table, "tag" }, vals );
    ins.execute();
}

int GetInstance::get_instance()
{
    return benchmark::get_instance( _conn, _config_id );
}

bool ImportModel::dedup()
{
    int candidate = _revision - 1;
    std::stringstream q;
    q << "select id, script from model where name = ? and revision = ? and variant";
    if ( _variant.empty() )
        q << " is null";
    else
        q << " = ?";

    nanodbc::statement get_script( _conn, q.str() );
    get_script.bind( 0, _name.c_str() );
    get_script.bind( 1, &candidate );
    if ( !_variant.empty() )
        get_script.bind( 2, _variant.c_str() );

    auto scr_id = get_script.execute();

    if ( !scr_id.first() )
        return false;
    if ( scr_id.get< int >( 1 ) != _script_id )
        return false;
    _id = scr_id.get< int >( 0 );

    nanodbc::statement get_files( _conn, "select filename, source from model_srcs where model = ?" );
    get_files.bind( 0, &_id );

    auto file = get_files.execute();
    int match = 0, indb = 0;
    while ( file.next() )
    {
        auto k = std::make_pair( file.get< std::string >( 0 ), file.get< int >( 1 ) );
        if ( _file_ids.count( k ) )
            ++ match;
        ++ indb;
    }

    if ( match == indb && match == int( _file_ids.size() ) )
    {
        std::cerr << "not imported: " << ident( false ) << " same as revision " << candidate << std::endl;
        return true;
    }

    return false;
}

std::string ImportModel::ident( bool rev )
{
    std::stringstream str;
    str << _name;
    if ( !_variant.empty() )
        str << " " << _variant;
    if ( rev )
        str << ", revision " << _revision;
    return str.str();
}

void ImportModel::import()
{
    if ( _interp.empty() )
        return;

    nanodbc::transaction txn( _conn );
    get_revision();

    auto r = brick::proc::spawnAndWait( brick::proc::CaptureStdout, { _interp, "--export", _path } );
    if ( !r )
    {
        std::cerr << "failed to export " << ident() << std::endl;
        return;
    }

    std::istringstream istr( r.out() );
    std::string script, line;
    std::map< std::string, std::string > files;

    while ( std::getline( istr, line ) )
    {
        if ( brick::string::startsWith( line, "load" ) )
        {
            int split = line.rfind( ' ' );
            std::string path( line, 5, split - 5 ),
                        name( line, split + 1, line.size() - split - 1 );
            files.emplace( name, path );
        }
        else script += line + "\n";
    }

    _script_id = put_file( "", script );
    for ( auto file : files )
        put_file( file.first, brick::fs::readFile( file.second ) );

    if ( dedup() )
        return;

    try {
        do_import();
        txn.commit();
        std::cerr << "imported " << ident() << std::endl;
    } catch ( database_error &err )
    {
        std::cerr << "import failed: " << ident() << std::endl;
        std::cerr << err.what() << std::endl;
    }
}

void ImportModel::do_import()
{
    odbc::Keys keys_mod{ "name", "revision", "script" };
    odbc::Vals vals_mod{ _name, _revision, _script_id };
    if ( !_variant.empty() )
        keys_mod.push_back( "variant" ), vals_mod.push_back( _variant );
    _id = odbc::unique_id( _conn, "model", keys_mod, vals_mod );

    for ( auto p : _file_ids )
    {
        odbc::Keys keys_tie{ "model", "filename", "source" };
        odbc::Vals vals_tie{ _id, p.first, p.second };
        auto ins = odbc::insert( _conn, "model_srcs", keys_tie, vals_tie );
        nanodbc::execute( ins );
    };
}

int ImportModel::put_file( std::string name, std::string src )
{
    auto sha = brick::sha2::to_hex( brick::sha2_256( src ) );
    std::vector< uint8_t > data( src.begin(), src.end() );
    int id = odbc::unique_id( _conn, "source", odbc::Keys{ "text", "sha" },
                              odbc::Vals{ data, sha } );
    if ( !name.empty() )
        _file_ids.emplace( name, id );
    return id;
}

void ImportModel::get_revision()
{
    std::stringstream rev_q;
    rev_q << "select max(revision) from model group by name, variant having name = ? and variant "
          << (_variant.empty() ? "is null" : "= ?");
    nanodbc::statement rev( _conn, rev_q.str() );
    rev.bind( 0, _name.c_str() );
    if ( !_variant.empty() )
        rev.bind( 1, _variant.c_str() );

    _revision = 1;
    try
    {
        auto r = nanodbc::execute( rev );
        r.first();
        _revision = 1 + r.get< int >( 0 );
    } catch (...) {}
}

void ImportModel::tag()
{
    if ( !_id ) return; /* no can do */
    nanodbc::transaction txn( _conn );
    nanodbc::statement clear( _conn, "delete from model_tags where model = ?" );
    clear.bind( 0, &_id );
    clear.execute();

    for ( auto tag : _tags )
        add_tag( "model", _id, tag );

    txn.commit();
}

void Import::run()
{
    std::string yaml;
    std::getline( std::cin, yaml, '\0' );
    brick::yaml::Parser parsed( yaml );
    std::vector< std::string > names;
    names = parsed.getOr( { "*" }, names );

    for ( auto name : names )
    {
        ImportModel im( _conn );
        im._name = parsed.get< std::string >( { name, "name" } );
        im._variant = parsed.get< std::string >( { name, "variant" } );
        im._path = parsed.get< std::string >( { name, "path" } );
        im._interp = parsed.get< std::string >( { name, "interpreter" } );
        im._tags = parsed.get< std::vector< std::string > >( { name, "tags", "*" } );
        im.import();
        im.tag(); /* update tags, even if the import failed */
    }
}

void Setup::run()
{
    int build = odbc::get_build( _conn );
    int inst = get_instance();
    std::cerr << "build " << build << ", instance " << inst << std::endl;
    if ( !_tag.empty() )
        add_tag( "build", build, _tag );
}

void Schedule::run()
{
    int inst = get_instance();
    std::cerr << "instance = " << inst << std::endl;

    std::stringstream q;
    /* XXX check that highest id is always the latest revision? */
    q << "select max( model.id ), model.name from model "
      << "join model_tags on model.id = model_tags.model "
      << "join tag on tag.id = model_tags.tag ";
    if ( !_tag.empty() )
        q << " where tag.name = ? ";
    q << "group by model.name, model.variant ";

    std::cerr << q.str() << std::endl;
    nanodbc::statement find( _conn, q.str() );
    if ( !_tag.empty() )
        find.bind( 0, _tag.c_str() );

    auto mod = find.execute();
    while ( mod.next() )
    {
        nanodbc::transaction txn( _conn );
        int mod_id = mod.get< int >( 0 );

        if ( _once )
        {
            nanodbc::statement sel( _conn, "select count(*) from job where model = ? and instance = ?" );
            sel.bind( 0, &mod_id );
            sel.bind( 1, &inst );
            auto res = sel.execute(); res.first();
            if ( res.get< int >( 0 ) )
                continue;
        }

        nanodbc::statement ins( _conn, "insert into job ( model, instance, status ) "
                                       "values (?, ?, 'P')" );
        ins.bind( 0, &mod_id );
        ins.bind( 1, &inst );
        ins.execute();
        try {
            txn.commit();
            std::cerr << "scheduled " << mod.get< std::string >( 1 ) << std::endl;
        } catch ( nanodbc::database_error & ) {};
    }
}

void Cmd::setup()
{
    try
    {
        if ( _odbc.empty() )
            _odbc = std::getenv( "DIVBENCH_DB" );
        _conn.connect( _odbc );
    }
    catch ( nanodbc::database_error &err )
    {
        std::cerr << "could not connect to " << _odbc << std::endl
                  << err.what() << std::endl;
    }
}

}
