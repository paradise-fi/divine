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

namespace benchmark
{

void Report::format( nanodbc::result res, odbc::Keys cols )
{
    std::cerr << "format: " << res.columns() << " columns, " << res.rows() << " rows" << std::endl;
    std::vector< size_t > _width( res.columns() );

    for ( size_t i = 0; i < cols.size(); ++i )
        _width[ i ] = cols[ i ].size();

    while ( res.next() )
        for ( int i = 0; i < res.columns(); ++ i )
            _width[ i ] = std::max( _width[ i ], res.get< std::string >( i ).size() );

    for ( size_t i = 0; i < cols.size(); ++i )
        std::cout << std::setw( _width[ i ] ) << cols[ i ] << " | ";
    std::cout << std::endl;

    for ( int i = 0; i < res.columns(); ++i )
        std::cout << std::string( _width[ i ], '-' ) << ( i == res.columns() - 1 ? "-|" : "-|-" );
    std::cout << std::endl;

    res.first(); do
    {
        for ( int i = 0; i < res.columns(); ++ i )
            std::cout << std::setw( _width[ i ] ) << res.get< std::string >( i ) << " | ";
        std::cout << std::endl;
    } while ( res.next() );
}

void Report::list_instances()
{
    nanodbc::statement find( _conn,
            "select instance.id, build.version, substr( build.source_sha, 0, 7 ), "
            "substr( build.runtime_sha, 0, 7 ), build.build_type, cpu.model, machine.cores, "
            "machine.mem / (1024 * 1024), "
            "(select count(*) from job where job.instance = instance.id and job.status = 'D') "
            "from instance join build join machine join cpu "
            "on instance.machine = machine.id "
            "and instance.build = build.id and cpu.id = machine.cpu" );
    format( find.execute(), odbc::Keys{ "instance", "version", "src", "rt", "build", "cpu", "cores", "mem", "jobs" } );
}

void Report::results()
{
    if ( _watch )
        std::cout << char( 27 ) << "[2J" << char( 27 ) << "[;H";

    std::stringstream q;
    q << "select instance.id, model.name, "
      << "(select max(states) from search_log where execution = execution.id ), "
      << "execution.result "
      << "from execution join instance join job join model "
      << "on instance.id = execution.instance "
      << "and job.execution = execution.id and job.model = model.id "
      << "where (";

    for ( size_t i = 0; i < _result.size(); ++i )
        q << "result = '" << _result[ i ] << ( i + 1 == _result.size() ? "') " : "' or " );
    if ( _instance >= 0 )
        q << " and instance.id = " << _instance;

    std::cerr << q.str() << std::endl;
    nanodbc::statement find( _conn, q.str() );
    format( find.execute(), odbc::Keys{ "instance", "model", "states", "result" } );

    if ( _watch )
    {
        sleep( 1 );
        return results();
    }
}

}
