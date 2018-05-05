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

#include <tools/divbench.hpp>
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
    std::vector< int > _width;
    std::vector< std::function< std::string( Value ) > > _format;

    template< typename... Args >
    void cols( Args... args )
    {
        for ( std::string c : { args... } )
            _cols.push_back( c );
        _format.resize( _cols.size() );
    }

    template< typename... Args >
    void intcols( Args... ic ) { for ( std::string i : { ic... } ) _intcols.insert( i ); }

    template< typename... Args >
    void timecols( Args... tc ) { for ( std::string t : { tc... } ) _timecols.insert( t ); }

    template< typename F >
    void set_format( std::string col, F f )
    {
        for ( int c = 0; c < int( _cols.size() ); ++ c )
            if ( _cols[ c ] == col )
                _format[ c ] = f;
    }

    void sum()
    {
        if ( !_rows.size() )
            return;
        Row total = _rows[ 0 ];
        for ( int r = 1; r < int( _rows.size() ); ++ r )
            for ( unsigned c = 0; c < _rows[ r ].size(); ++c )
                _rows[ r ][ c ].match( [&]( std::string ) { total[ c ] = std::string(); },
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
            {
                if ( res.is_null( c ) )
                    r.emplace_back( "-" );
                else if ( _intcols.count( _cols[ c ] ) )
                    r.emplace_back( res.get< int >( c ) );
                else if (  _timecols.count( _cols[ c ] ) )
                    r.emplace_back( MSecs( res.get< int >( c ) ) );
                else
                    r.emplace_back( res.get< std::string >( c ) );
            }
            _rows.push_back( r );
        }
    }

    std::string format( int c, Value v, int w )
    {
        ASSERT_LT( c, _format.size() );
        if ( _format[ c ] )
            return _format[ c ]( v );
        return format( v, w );
    }

    std::string format( Value v, int w )
    {
        std::stringstream str;
        v.match( [&]( MSecs m ) { str << interval_str( m ); },
                 [&]( int i )
                 {
                     if ( i >= 10000 )
                     {
                         int prec = 1;
                         std::string s = "k";
                         auto f = double( i ) / 1000;
                         if ( f >= 10000 ) f /= 1000, s = "M";
                         else if ( f >= 1000 ) prec = 0;
                         str << std::fixed << std::setprecision( prec ) << f << " " << s;
                     }
                     else
                         str << i;
                 },
                 [&]( std::string s ) { str << s << ( w ? std::string( w - s.size(), ' ' ) : "" ); } );
        return str.str();
    }

    int width( int c )
    {
        int w = _cols[ c ].size();
        return w;
    }

    template< typename T >
    void format_row( std::ostream &o, T r, bool use_fmt = true )
    {
        o << "| ";
        for ( unsigned c = 0; c < _cols.size(); ++c )
        {
            auto w = ( _intcols.count( _cols[ c ] ) || _timecols.count( _cols[ c ] ) ) ? 0 : _width[ c ];
            o << std::setw( _width[ c ] ) << ( use_fmt ? format( c, r[ c ], w ) : format( r[c], w ) );
            if ( c + 1 != _cols.size() )
                o << " | ";
        }
        o << " |" << std::endl;
    }

    void compute_widths()
    {
        _width.clear();
        std::transform( _cols.begin(), _cols.end(), std::back_inserter( _width ),
                        []( std::string c ) { return c.size(); } );
        for ( unsigned c = 0; c < _cols.size(); ++c )
            for ( auto r : _rows )
                _width[ c ] = std::max( _width[ c ], int( format( c, r[ c ], 0 ).size() ) );
    }

    void format( std::ostream &o )
    {
        compute_widths();
        format_row( o, _cols, false );
        o << "|-";
        for ( unsigned c = 0; c < _cols.size(); ++c )
            o << std::string( _width[ c ], '-' ) << ( c + 1 == _cols.size() ? "-|" : "-|-" );
        o << std::endl;
        for ( auto r : _rows )
            format_row( o, r );
    }
};

void ReportBase::find_instances()
{
    for ( auto &group : _instances )
        find_instances( group );
}

void ReportBase::find_instances( std::vector< std::string > tags )
{
    std::stringstream q;
    q << "select distinct instance.id from instance ";

    for ( unsigned i = 0; i < tags.size(); ++i )
        q << "left join config_tags as c" << i << " on c" << i << ".config = instance.config "
          << "left join machine_tags as m" << i << " on m" << i << ".machine = instance.machine "
          << "left join build_tags as b" << i << " on b" << i << ".build = instance.build "
          << "left join tag as ct" << i << " on ct" << i << ".id = c" << i << ".tag "
          << "left join tag as mt" << i << " on mt" << i << ".id = m" << i << ".tag "
          << "left join tag as bt" << i << " on bt" << i << ".id = b" << i << ".tag ";

    q << " where true ";

    for ( unsigned i = 0; i < tags.size(); ++i )
        q << " and ( ct" << i << ".name = ? or mt" << i << ".name = ? or bt" << i << ".name = ? )";

    std::cerr << q.str() << std::endl;
    nanodbc::statement sel( _conn, q.str() );
    for ( unsigned i = 0; i < tags.size(); ++i )
    {
        sel.bind( 3 * i + 0, tags[ i ].c_str() );
        sel.bind( 3 * i + 1, tags[ i ].c_str() );
        sel.bind( 3 * i + 2, tags[ i ].c_str() );
    }

    auto r = sel.execute();
    while ( r.next() )
        _instance_ids.push_back( r.get< int >( 0 ) );
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
    nanodbc::statement find( _conn, q.str() );
    Table res;
    res.cols( "instance", "version", "src", "rt", "build", "cpu", "cores", "mem", "jobs" );
    res.intcols( "instance" );
    res.set_format( "cpu", [=]( Table::Value v )
                    {
                        auto s = v.get< std::string >();
                        std::regex tidy( "Intel\\(R\\) (Xeon)\\(R\\) CPU *"
                                         "|Intel\\(R\\) (Core)\\(TM\\) " );
                        return std::regex_replace( s, tidy, "$1 " );
                    } );
    res.set_format( "build", []( Table::Value v )
                    {
                        auto s = v.get< std::string >();
                        if ( s == "RelWithDebInfo" ) return std::string( "RWDI" );
                        return s;
                    } );
    res.fromSQL( find.execute() );
    res.format( std::cout );
}

void Report::results()
{
    find_instances();

    if ( _instance_ids.size() != 1 )
        throw brick::except::Error( "Need exactly one instance, but " +
                                    brick::string::fmt( _instance_ids.size() ) + " matched" );

    if ( _watch )
        std::cout << char( 27 ) << "[2J" << char( 27 ) << "[;H";

    std::stringstream q;
    q << "select instid, name, "
      << ( _by_tag ? "count( modid ), " : "coalesce(variant, ''), result, " )
      << ( _by_tag ? "sum( states )" : "states" ) << ", "
      << ( _by_tag ? "sum(time_search), sum(time_ce), " : "time_search, time_ce, " )
      << ( _by_tag ? "(count( modid ) - sum(cast(correct as integer))) as wrong" : "correct" )
      << " from (select instance.id as instid, "
      << ( _by_tag ? "tag.name" : "model.name" ) << " as name, "
      << _agg << "(states) as states, model.id as modid, variant, "
      << _agg << "(time_search) as time_search, "
      << _agg << "(time_ce) as time_ce, min(result) as result, "
      << "bool_and(correct) as correct "
      << "from execution "
      << "join job on job.execution = execution.id "
      << "join instance on job.instance = instance.id "
      << "join model on job.model = model.id ";
    if ( _by_tag )
      q << "join model_tags on model_tags.model = model.id "
        << "join tag on model_tags.tag = tag.id ";
    q << " where ( ";

    for ( size_t i = 0; i < _result.size(); ++i )
        q << "result = '" << _result[ i ] << ( i + 1 == _result.size() ? "' ) " : "' or " );
    q << " and instance.id = " << _instance_ids[ 0 ];
    q << " and correct is not null ";
    for ( auto t : _tag )
    {
        q << " and ( select count(*) from model_tags as mt join tag on mt.tag = tag.id"
          << "       where tag.name = ? and mt.model = model.id) > 0 ";
    }
    q << " group by instance.id, model.id" << (_by_tag ? ", tag.id" : "") << " ) as l ";
    if ( _by_tag )
        q << " group by name, instid";
    q << " order by name";

    nanodbc::statement find( _conn, q.str() );

    for ( unsigned i = 0; i < _tag.size(); ++i )
        find.bind( i, _tag[ i ].c_str() );

    Table res;

    if ( _by_tag )
        res.cols( "instance", "tag", "models", "states", "search", "ce", "wrong" );
    else
        res.cols( "instance", "model", "variant", "result", "states", "search", "ce", "correct" );

    res.timecols( "search", "ce" );
    res.intcols( "models", "states" );
    res.fromSQL( find.execute() );
    res.sum();
    res.format( std::cout );

    if ( _watch )
    {
        sleep( 1 );
        return results();
    }
}

void Compare::run()
{
    find_instances();

    if ( _fields.empty() )
        _fields = { "time_search", "states" };
    if ( _instance_ids.empty() )
        throw brick::except::Error( "At least one --instance must be specified." );

    std::stringstream q;
    std::string x0 = "x" + std::to_string( _instance_ids[ 0 ] );

    if ( _by_tag )
        q << "select tag.name, count(" << x0 << ".modid) ";
    else
        q << "select " << x0 << ".modname, " << x0 << ".modvar ";

    for ( auto f : _fields )
        for ( auto i : _instance_ids )
            q << ", " << ( _by_tag ? "sum" : "" ) << "(x" << i << "." << f << ") ";
    q << " from ";

    for ( auto it = _instance_ids.begin(); it != _instance_ids.end(); ++it )
    {
        q << "(select model.id as modid, coalesce(model.variant, '') as modvar, model.name as modname";
        for ( auto f : _fields )
            q << ", " << _agg << "(" << f << ") as " << f << " ";
        q << " from model join job on model.id = job.model"
          << " join execution on job.execution = execution.id "
          << " where job.instance = ? and job.status = 'D' and execution.correct and ( ";
        for ( size_t i = 0; i < _result.size(); ++i )
            q << "result = '" << _result[ i ] << ( i + 1 == _result.size() ? "' ) " : "' or " );
        q << " group by model.id) as x" << *it << " ";
        if ( it != _instance_ids.begin() )
            q << " on x" << *std::prev( it ) << ".modid = x" << *it << ".modid ";
        if ( std::next( it ) != _instance_ids.end() )
            q << " join ";
    }
    if ( _by_tag )
        q << " join model_tags on model_tags.model = " << x0 << ".modid"
          << " join tag on model_tags.tag = tag.id";
    for ( auto t : _tag )
        q << " where ( select count(*) from model_tags as mt join tag on mt.tag = tag.id "
          << " where mt.model = " << x0 << ".modid and tag.name = ? ) > 0";

    if ( _by_tag )
        q << " group by tag.id";
    else
        q << " order by " << x0 << ".modname";

    std::cerr << q.str() << std::endl;

    Table res;
    if ( _by_tag )
        res.cols( "tag", "models" ), res.intcols( "models" );
    else
        res.cols( "model", "variant" );

    for ( auto f : _fields )
        for ( auto i : _instance_ids )
        {
            auto k = std::to_string( i ) + "/" + f;
            res.cols( k );
            if ( brick::string::startsWith( f, "time" ) )
                res.timecols( k );
            if ( f == "states" )
                res.intcols( k );
        }

    nanodbc::statement find( _conn, q.str() );
    for ( int i = 0; i < int( _instance_ids.size() ); ++i )
        find.bind( i, &_instance_ids[ i ] );
    for ( unsigned i = 0; i < _tag.size(); ++i )
        find.bind( _instance_ids.size() + i, _tag[ i ].c_str() );

    res.fromSQL( find.execute() );
    res.sum();
    res.format( std::cout );
}

}
