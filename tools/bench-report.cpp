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
#include <divine/ui/util.hpp>

namespace benchmark
{

using namespace divine::ui;

void format( nanodbc::result res, odbc::Keys cols, std::set< std::string > tcols = {} )
{
    std::cerr << "format: " << res.columns() << " columns, " << res.rows() << " rows" << std::endl;
    std::vector< size_t > _width( res.columns() );

    for ( size_t i = 0; i < cols.size(); ++i )
        _width[ i ] = cols[ i ].size();

    while ( res.next() )
        for ( int i = 0; i < res.columns(); ++ i )
            if ( !tcols.count( cols[ i ] ) )
                 _width[ i ] = std::max( _width[ i ], res.get< std::string >( i ).size() );
            else
                 _width[ i ] = std::max( _width[ i ],
                                         interval_str( MSecs( res.get< int >( i ) ) ).size() );

    std::cout << "| ";
    for ( size_t i = 0; i < cols.size(); ++i )
        std::cout << std::setw( _width[ i ] ) << cols[ i ] << " | ";
    std::cout << std::endl;

    std::cout << "|-";
    for ( int i = 0; i < res.columns(); ++i )
        std::cout << std::string( _width[ i ], '-' ) << ( i == res.columns() - 1 ? "-|" : "-|-" );
    std::cout << std::endl;

    res.first(); do
    {
        std::cout << "| ";
        for ( int i = 0; i < res.columns(); ++ i )
            if ( tcols.count( cols[ i ] ) )
                std::cout << std::setw( _width[ i ] )
                          << interval_str( MSecs( res.get< int >( i ) ) ) << " | ";
            else
                std::cout << std::setw( _width[ i ] ) << res.get< std::string >( i ) << " | ";
        std::cout << std::endl;
    } while ( res.next() );
}

void Report::list_instances()
{
    std::stringstream q;
    q << "select "
      << "  instance.id, build.version, substr( build.source_sha, 0, 7 ), "
      << "  substr( build.runtime_sha, 0, 7 ), build.build_type, "
      << "  cpu.model, machine.cores, "
      << "  machine.mem / (1024 * 1024), "
      << "  (select count(*) from job where job.instance = instance.id and job.status = 'D') "
      << "from instance "
      << "join build   on instance.build = build.id "
      << "join machine on instance.machine = machine.id "
      << "join cpu     on machine.cpu = cpu.id ";
    odbc::Keys keys{ "instance", "version", "src", "rt", "build", "cpu", "cores", "mem", "jobs" };
    nanodbc::statement find( _conn, q.str() );
    format( find.execute(), keys );
}

void Report::results()
{
    if ( _watch )
        std::cout << char( 27 ) << "[2J" << char( 27 ) << "[;H";

    std::stringstream q;
    q << "select instance.id, " << ( _by_tag ? "tag.name" : "model.name" ) << ", "
      << ( _by_tag ? "sum( states ), " : "states, " )
      << ( _by_tag ? "count( model.id )" : "execution.result" ) << ", "
      << ( _by_tag ? "sum(time_search), sum(time_ce)" : "time_search, time_ce" ) << " "
      << "from execution "
      << "join instance on execution.instance = instance.id "
      << "join job on job.execution = execution.id "
      << "join model on job.model = model.id "
      << ( _by_tag ? "join model_tags on model_tags.model = model.id " : "" )
      << ( _by_tag ? "join tag on model_tags.tag = tag.id " : "" )
      << " where ( ";
    for ( size_t i = 0; i < _result.size(); ++i )
        q << "result = '" << _result[ i ] << ( i + 1 == _result.size() ? "' ) " : "' or " );
    q << ( _by_tag ? " group by tag.id, instance.id " : "" );
    if ( _instance >= 0 )
        q << ( _by_tag ? " having " : " and " ) << " instance.id = " << _instance;
    q << " order by " << ( _by_tag ? "tag.name" : "model.name" );

    std::cerr << q.str() << std::endl;
    nanodbc::statement find( _conn, q.str() );
    odbc::Keys hdr{ "instance", _by_tag ? "tag" : "model", "states",
                    _by_tag ? "models" : "result", "search", "ce" };
    format( find.execute(), hdr, { "search"s, "ce"s } );

    if ( _watch )
    {
        sleep( 1 );
        return results();
    }
}

void Compare::run()
{
    std::stringstream q;
    q << "select " << ( _by_tag ? "tag.name" : "model.name" ) << ", "
      << ( _by_tag ? "sum(ex1.time_search), sum(ex2.time_search) " :
                     "ex1.time_search, ex2.time_search " )
      << "from model "
      << ( _by_tag ? " join model_tags on model.id = model_tags.model " : "" )
      << ( _by_tag ? " join tag on tag.id = model_tags.tag " : "" )
      << "join job as job1 on job1.model = model.id "
      << "join job as job2 on job2.model = job1.model "
      << "join execution as ex1 on job1.execution = ex1.id "
      << "join execution as ex2 on job2.execution = ex2.id "
      << "where job1.instance = ? and job2.instance = ? "
      << "and job1.status = 'D' and job2.status = 'D' ";
    if ( _by_tag )
        q << "group by tag.name";
    nanodbc::statement find( _conn, q.str() );
    find.bind( 0, &_instances[ 0 ] );
    find.bind( 1, &_instances[ 1 ] );
    std::cerr << brick::string::fmt_container( _instances, "{", "}" ) << std::endl;
    std::cerr << q.str() << std::endl;
    format( find.execute(), odbc::Keys{ "model", "1/search", "2/search" },
            { "1/search"s, "2/search"s } );
}

}
