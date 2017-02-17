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

struct Table
{
    using Value = brick::types::Union< int, MSecs, std::string >;
    using Row = std::vector< Value >;
    std::vector< Row > _rows;
    std::vector< std::string > _cols;
    std::set< std::string > _intcols, _timecols;

    template< typename... Args >
    void cols( Args... args )
    {
        for ( std::string c : { args... } )
            _cols.push_back( c );
    }

    template< typename... Args >
    void intcols( Args... ic ) { for ( std::string i : { ic... } ) _intcols.insert( i ); }

    template< typename... Args >
    void timecols( Args... tc ) { for ( std::string t : { tc... } ) _timecols.insert( t ); }

    void sum()
    {
        ASSERT_LT( 0, _rows.size() );
        Row total = _rows[ 0 ];
        for ( auto row : _rows )
            for ( unsigned c = 0; c < row.size(); ++c )
                row[ c ].match( [&]( std::string ) { total[ c ] = std::string(); },
                                [&]( auto v ) { total[ c ].get< decltype( v ) >() += v; } );
        total[ 0 ] = std::string( "**total**" );
        _rows.push_back( total );
    }

    void fromSQL( nanodbc::result res )
    {
        ASSERT_EQ( _cols.size(), res.columns() );
        while ( res.next() )
        {
            Row r;
            for ( unsigned c = 0; c < _cols.size(); ++c )
                if ( _intcols.count( _cols[ c ] ) )
                    r.emplace_back( res.get< int >( c ) );
                else if (  _timecols.count( _cols[ c ] ) )
                    r.emplace_back( MSecs( res.get< int >( c ) ) );
                else
                    r.emplace_back( res.get< std::string >( c ) );
            _rows.push_back( r );
        }
    }

    std::string format( Value v )
    {
        std::string rv;
        v.match( [&]( MSecs m ) { rv = interval_str( m ); },
                 [&]( int i ) { rv = std::to_string( i ); },
                 [&]( std::string s ) { rv = s; } );
        return rv;
    }

    int width( int c )
    {
        int w = _cols[ c ].size();
        for ( auto r : _rows )
            w = std::max( w, int( format( r[ c ] ).size() ) );
        return w;
    }

    template< typename T >
    void format_row( std::ostream &o, T r )
    {
        o << "| ";
        for ( unsigned c = 0; c < _cols.size(); ++c )
        {
            o << std::setw( width( c ) ) << format( r[ c ] );
            if ( c + 1 != _cols.size() )
                o << " | ";
        }
        o << " |" << std::endl;
    }

    void format( std::ostream &o )
    {
        format_row( o, _cols );
        o << "|-";
        for ( unsigned c = 0; c < _cols.size(); ++c )
            o << std::string( width( c ), '-' ) << ( c + 1 == _cols.size() ? "-|" : "-|-" );
        o << std::endl;
        for ( auto r : _rows )
            format_row( o, r );
    }
};

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
    nanodbc::statement find( _conn, q.str() );
    Table res;
    res.cols( "instance", "version", "src", "rt", "build", "cpu", "cores", "mem", "jobs" );
    res.fromSQL( find.execute() );
    res.format( std::cout );
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

    Table res;

    if ( _by_tag )
        res.cols( "instance", "tag", "models", "states", "search", "ce" );
    else
        res.cols( "instance", "model", "variant", "result", "states", "search", "ce" );

    res.timecols( "search", "ce" );
    res.intcols( "models", "states" );
    res.fromSQL( find.execute() );
    res.format( std::cout );

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
    if ( _by_tag )
        q << "select tag.name ";
    else
        q << "select x" << _instances[ 0 ] << ".modname ";

    for ( auto f : _fields )
        for ( auto i : _instances )
            q << ", " << ( _by_tag ? "sum" : "" ) << "(x" << i << "." << f << ") ";
    q << " from ";

    for ( auto it = _instances.begin(); it != _instances.end(); ++it )
    {
        q << "(select model.id as modid, model.name as modname";
        for ( auto f : _fields )
            q << ", " << _agg << "(" << f << ") as " << f << " ";
        q << " from model join job on model.id = job.model"
          << " join execution on job.execution = execution.id "
          << " where job.instance = ? and job.status = 'D' and ( ";
        for ( size_t i = 0; i < _result.size(); ++i )
            q << "result = '" << _result[ i ] << ( i + 1 == _result.size() ? "' ) " : "' or " );
        q << " group by model.id) as x" << *it << " ";
        if ( it != _instances.begin() )
            q << " on x" << *std::prev( it ) << ".modid = x" << *it << ".modid ";
        if ( std::next( it ) != _instances.end() )
            q << " join ";
    }
    if ( _by_tag )
        q << " join model_tags on model_tags.model = x" << _instances[ 0 ] << ".modid"
          << " join tag on model_tags.tag = tag.id group by tag.id";

    Table res;
    res.cols( "model" );

    for ( auto f : _fields )
        for ( auto i : _instances )
        {
            auto k = std::to_string( i ) + "/" + f;
            res.cols( k );
            if ( brick::string::startsWith( f, "time" ) )
                res.timecols( k );
            if ( f == "states" )
                res.intcols( k );
        }

    nanodbc::statement find( _conn, q.str() );
    for ( int i = 0; i < _instances.size(); ++i )
        find.bind( i, &_instances[ i ] );

    res.fromSQL( find.execute() );
    res.format( std::cout );
}

}
