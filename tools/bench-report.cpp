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
    q << "select instid, name, "
      << ( _by_tag ? "count( modid ), " : "coalesce(variant, ''), result, " )
      << ( _by_tag ? "sum( states )" : "states" ) << ", "
      << ( _by_tag ? "sum(time_search), sum(time_ce)" : "time_search, time_ce" ) << " "
      << "from (select instance.id as instid, "
      << ( _by_tag ? "tag.name" : "model.name" ) << " as name, "
      << _agg << "(states) as states, model.id as modid, variant, "
      << _agg << "(time_search) as time_search, "
      << _agg << "(time_ce) as time_ce, min(result) as result "
      << "from execution join instance on execution.instance = instance.id "
      << "join job on job.execution = execution.id "
      << "join model on job.model = model.id ";
    if ( _by_tag )
      q << "join model_tags on model_tags.model = model.id "
        << "join tag on model_tags.tag = tag.id ";
    q << " where ( ";

    for ( size_t i = 0; i < _result.size(); ++i )
        q << "result = '" << _result[ i ] << ( i + 1 == _result.size() ? "' ) " : "' or " );
    if ( _instance >= 0 )
        q << " and instance.id = " << _instance;
    q << " group by instance.id, model.id" << (_by_tag ? ", tag.id" : "") << ") as l ";
    if ( _by_tag )
        q << " group by name, instid";
    q << " order by name";

    nanodbc::statement find( _conn, q.str() );

    odbc::Keys hdr;
    if ( _by_tag )
        hdr = odbc::Keys{ "instance", "tag", "models", "states", "search", "ce" };
    else
        hdr = odbc::Keys{ "instance", "model", "variant", "result", "states", "search", "ce" };
    format( find.execute(), hdr, { "search"s, "ce"s } );

    if ( _watch )
    {
        sleep( 1 );
        return results();
    }
}

void Compare::run()
{
    if ( _fields.empty() )
        _fields = { "time_search", "states" };

    std::stringstream q;
    q << "select " << ( _by_tag ? "tag.name" : "model.name" );
    for ( auto f : _fields )
        for ( auto i : _instances )
            q << ", " << ( _by_tag ? "sum" : "" ) << "(x" << i << "." << f << ") ";
    q << " from ";

    for ( auto it = _instances.begin(); it != _instances.end(); ++it )
    {
        q << "(select model.id as modid";
        for ( auto f : _fields )
            q << ", " << _agg << "(" << f << ") as " << f << " ";
        q << " from model join job on model.id = job.model"
          << " join execution on job.execution = execution.id "
          << " where job.instance = ? and job.status = 'D' "
          << " group by model.id) as x" << *it << " ";
        if ( it != _instances.begin() )
            q << " on x" << *std::prev( it ) << ".modid = x" << *it << ".modid ";
        if ( std::next( it ) != _instances.end() )
            q << " join ";
    }
    if ( _by_tag )
        q << " join model_tags on model_tags.model = x" << _instances[ 0 ] << ".modid"
          << " join tag on model_tags.tag = tag.id group by tag.id";

    odbc::Keys hdr{ "model" };
    std::set< std::string > tf;

    for ( auto f : _fields )
        for ( auto i : _instances )
        {
            auto k = std::to_string( i ) + "/" + f;
            hdr.push_back( k );
            if ( brick::string::startsWith( f, "time" ) )
                 tf.insert( k );
        }

    nanodbc::statement find( _conn, q.str() );
    for ( int i = 0; i < _instances.size(); ++i )
        find.bind( i, &_instances[ i ] );

    std::cerr << brick::string::fmt_container( _instances, "{", "}" ) << std::endl;

    format( find.execute(), hdr, tf );
}

}
