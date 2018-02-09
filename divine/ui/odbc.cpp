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

#if OPT_SQL

#include <divine/ui/odbc.hpp>
#include <divine/ui/sysinfo.hpp>

using namespace divine::ui;
using namespace divine::ui::odbc;

using brick::string::fmt_container;
using brick::types::Union;

extern const char *DIVINE_VERSION;
extern const char *DIVINE_SOURCE_SHA;
extern const char *DIVINE_RUNTIME_SHA;
extern const char *DIVINE_BUILD_TYPE;
extern const char *DIVINE_RELEASE_SHA;

namespace divine::ui::odbc
{

using namespace nanodbc;

void bind_vals( statement &stmt, Vals &vals )
{
    using BV = std::vector< uint8_t >;
    for ( int i = 0; i < int( vals.size() ); ++i )
        vals[ i ].match( [&]( const std::string &v ) { stmt.bind( i, v.c_str() ); },
                         [&]( const BV &v ) { stmt.bind( i, std::vector< BV >{ v } ); },
                         [&]( const int &v ) { stmt.bind( i, &v ); } );
}

statement insert( connection conn, std::string table, Keys keys, Vals &vals )
{
    std::stringstream q;
    q << "insert into " << table << " "
      << fmt_container( keys, "(", ")" )
      << " values "
      << fmt_container( Keys( keys.size(), "?" ), "(", ")" );
    statement s( conn, q.str() );
    bind_vals( s, vals );
    return s;
}

statement select_id( connection conn, std::string table, Keys keys, Vals &vals )
{
    std::stringstream q;

    for ( auto &key : keys )
        key += " = ?";

    q << "select min( id ) from " << table << " where "
      << fmt_container( keys, "", "", " AND " );

    statement s( conn, q.str() );
    bind_vals( s, vals );
    return s;
}

/* relies on unique constraints in the database */
int unique_id( connection conn, std::string table, Keys keys, Vals vals )
{
    ASSERT_EQ( keys.size(), vals.size() );

    statement ins = insert( conn, table, keys, vals ),
              sel = select_id( conn, table, keys, vals );
    int id;

    try { execute( ins ); }
    catch ( nanodbc::database_error &err ) {};

    auto rv = execute( sel );
    rv.first();
    id = rv.get< int >( 0 );

    return id;
}

int get_external_build( connection conn, ExternalBuildInfo &bi )
{
    Keys keys{ "version", "source_sha", "runtime_sha", "build_type", "is_release" };
    Vals vals{ bi.version, bi.checksum, bi.driver_checksum, bi.build_type, false };
    return unique_id( conn, "build", keys, vals );
}

int get_build( connection conn )
{
    std::string composite = std::string( DIVINE_SOURCE_SHA ) + " " + DIVINE_RUNTIME_SHA;

    Keys keys{ "version", "source_sha", "runtime_sha", "build_type", "is_release" };
    Vals vals{ DIVINE_VERSION, DIVINE_SOURCE_SHA, DIVINE_RUNTIME_SHA, DIVINE_BUILD_TYPE,
               composite == DIVINE_RELEASE_SHA };
    return unique_id( conn, "build", keys, vals );
}

int get_machine( connection conn )
{
    ui::SysInfo sys;

    Keys cpu_keys{ "model" };
    Vals cpu_vals{ sys.architecture() };
    int cpuid = unique_id( conn, "cpu", cpu_keys, cpu_vals );

    Keys mach_keys{ "cpu", "cores", "mem" };
    Vals mach_vals{ cpuid, std::thread::hardware_concurrency(), sys.memory() };
    return unique_id( conn, "machine", mach_keys, mach_vals );
}

int add_execution( connection conn )
{
    Keys keys; Vals vals; /* empty */
    auto ins = insert( conn, "execution", keys, vals );
    execute( ins );
    auto rv = execute( conn, "select last_insert_rowid()" );
    rv.first();
    return rv.get< int >( 0 );
}

void update_execution( nanodbc::connection conn, int id, const char *result, int lart, int load,
                       int boot, int search, int ce, int states )
{
    nanodbc::statement update( conn, "update execution set result = ?, time_lart = ?, "
                                     "time_load = ?, time_boot = ?, time_search = ?, "
                                     "time_ce = ?, states = ? where id = ?" );
    update.bind( 0, result );
    update.bind( 1, &lart );
    update.bind( 2, &load );
    update.bind( 3, &boot );
    update.bind( 4, &search );
    update.bind( 5, &ce );
    update.bind( 6, &states );
    update.bind( 7, &id );
    update.execute();
}

}

namespace divine::ui
{

struct ODBCSink : TimedSink
{
    nanodbc::connection _conn;
    int _execution = -1;
    int _pool_seq = 0, _prog_seq = 0, _states = 0;

    ODBCSink( std::string str ) : _conn( str )
    {
        _execution = odbc::add_execution( _conn );
    }

    void progress( std::pair< int64_t, int64_t > stat, int queued, bool last ) override
    {
        TimedSink::progress( stat, queued, last );
        ASSERT_LEQ( 0, _execution );
        Keys keys{ "seq", "execution", "states", "queued" };
        Vals vals{ _prog_seq++, _execution, stat.first, queued };
        auto ins = odbc::insert( _conn, "search_log", keys, vals );
        nanodbc::execute( ins );
        _states = stat.first;
    }

    void log_pool( const brick::mem::Stats &st, int pid )
    {
        for ( auto si : st )
        {
            odbc::Keys keys{ "seq", "execution", "pool", "items", "used", "held" };
            odbc::Vals vals{ _pool_seq++, _execution, pid,
                             si.count.used, si.bytes.used, si.bytes.held };
            auto ins = odbc::insert( _conn, "pool_log", keys, vals );
            ins.execute();
        }
    }

    void memory( const mc::PoolStats &st, bool ) override
    {
        for ( const auto &pool : st )
        {
            odbc::Keys keys{ "name" };
            odbc::Vals vals{ pool.first };
            int pid = odbc::unique_id( _conn, "pool", keys, vals );
            log_pool( pool.second, pid );
        }
    }

    void result( mc::Result r, const mc::Trace &trace ) override
    {
        TimedSink::result( r, trace );
        save_result( r );
    }

    void save_result( mc::Result r )
    {
        using mc::Result;
        const char *rchar;

        switch ( r )
        {
            case Result::None:      rchar = "U"; break;
            case Result::Error:     rchar = "E"; break;
            case Result::BootError: rchar = "B"; break;
            case Result::Valid:     rchar = "V"; break;
        }

        update_execution( _conn, _execution, rchar, _time_lart.count(),
                          _time_rr.count() + _time_const.count(),
                          _time_boot.count(), _time_search.count(),
                          _time_ce.count(), _states );
    }

    void set_result( mc::Result r, long states ) override
    {
        TimedSink::set_result( r, states );
        _states = states;
        save_result( r );
    }

    int log_id() override { return _execution; }
};

SinkPtr make_odbc( std::string connstr )
{
    return std::make_shared< ODBCSink >( connstr );
}

}

#endif /* OPT_SQL */
