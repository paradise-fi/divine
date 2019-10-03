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

#include <sql.h>
#include <sqlext.h>
#include <variant>
namespace benchmark
{

using namespace divine::ui;

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

struct ReportTable : ReportFormat
{
    using Type = ReportFormat::Field::Type;

    struct field_compare {
        bool operator()( const Field& a, const Field& b) const {
            return a.name < b.name;
        }
    };

    std::map< Field, size_t, field_compare > column_width;

    virtual ~ReportTable() = default;

    auto column( std::string name ) const noexcept
    {
        return std::find_if( fields.cbegin(), fields.cend(), [&] ( const auto & field ) {
            return field.name == name;
        } );
    }

    auto column( std::string name ) noexcept
    {
        return std::find_if( fields.begin(), fields.end(), [&] ( const auto & field ) {
            return field.name == name;
        } );
    }

    Record::Value record_value( const nanodbc::result & row, int col ) const noexcept
    {
        auto msec = [] ( int val ) { return std::chrono::milliseconds( val ); };

        if ( auto mem = column( row.column_name( col ) ); mem != fields.end() ) {
            if ( row.is_null( col ) )
                return Record::null_value();

            switch ( mem->type ) {
                case Field::Type::integer:
                    return row.get< int >( col );
                case Field::Type::time:
                    return msec( row.get< int >( col ) );
                case Field::Type::string:
                    return row.get< std::string >( col );
            }
        }

        return Record::null_value();
    }

    Record record_from_sql( const nanodbc::result & row ) const noexcept final
    {
        Record record;
        for ( int col = 0; col < row.columns(); ++col )
            record.add_member( row.column_name( col ), record_value( row, col ) );
        return record;
    }

    std::string format( const Record::Value & value ) const noexcept
    {
        std::stringstream ss;

        std::visit( overloaded {
            [&] ( std::chrono::milliseconds ms ) { ss << interval_str( ms ); },
            [&] ( int i ) {
                if ( i >= 10000 )
                {
                    int prec = 1;
                    std::string s = "k";
                    auto f = double( i ) / 1000;
                    if ( f >= 10000 ) f /= 1000, s = "M";
                    else if ( f >= 1000 ) prec = 0;
                    ss << std::fixed << std::setprecision( prec ) << f << " " << s;
                }
                else
                    ss << i;
            },
            [&] ( std::string s ) { ss << s; },
            [&] ( Record::null_value ) { ss << ""; }
        }, value );

        return ss.str();
    }

    void compute_widths() noexcept
    {
        for ( auto &field : fields )
            column_width[ field ] = field.name.size();

        for ( const auto & rec : records )
            for ( const auto & [name, value] : rec.data )
                if ( auto field = column( name ); field != fields.end() )
                    column_width[ *field ] = std::max( column_width[ *field ], format( value ).size() );
    }

    auto column_align( const Field & field ) const noexcept
    {
        if ( field.type == Field::Type::string )
            return std::left;
        return std::right;
    }

    std::string column_value( const Record &rec, const Field & field ) const noexcept
    {
        if ( rec.data.count( field.name ) )
            return format( rec.data.at( field.name ) );
        return "";
    };


    template< typename T >
    T sum_members( const std::string & member ) const noexcept
    {
        T sum{};
        for ( const auto & rec : records ) {
            if ( rec.data.count( member ) ) {
                auto val = rec.data.at( member );
                if ( std::holds_alternative< T >( val ) )
                    sum += std::get< T >( val );
            }
        }
        return sum;
    }

    Record summary() const
    {
        if ( !records.size() )
            return {};

        Record total;
        total.add_member( fields.begin()->name, "**total**" );
        for ( const auto & field : fields )
            if ( field.type == Type::integer )
                total.add_member( field.name, sum_members< int >( field.name ) );
            else if ( field.type == Type::time )
                total.add_member( field.name, sum_members< std::chrono::milliseconds >( field.name ) );

        return total;
    }

    void format( std::ostream &os ) noexcept
    {
        records.emplace_back( summary() );
        compute_widths();
        format_header( os );
        for ( const auto & rec : records )
           format_row( os, rec );
    }

    virtual void format_header( std::ostream &os ) const noexcept = 0;
    virtual void format_row( std::ostream &os, const Record &val ) const noexcept = 0;
};


struct Markdown final : ReportTable
{
    void format_header( std::ostream &o ) const noexcept
    {
        o << "|";
        for ( const auto & field : fields )
            o << " " << column_align( field ) << std::setw( column_width.at( field ) ) << field.name << " |";
        o << "\n|";
        for ( const auto & field : fields )
            o << '-' << std::string( column_width.at( field ), '-' ) << "-|";
        o << std::endl;
    }

    void format_row( std::ostream &o, const Record &rec ) const noexcept
    {
        o << "|";
        for ( const auto & field : fields )
            o << " " << column_align( field ) << std::setw( column_width.at( field ) )
              << column_value( rec, field ) << " |";
        o << std::endl;

    }
};

void ReportFormat::from_sql( nanodbc::result && res )
{
    while ( res.next() )
        records.emplace_back( this->record_from_sql( res ) );
}


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

std::string list_tags( nanodbc::connection &conn, std::string table, int id )
{
    std::stringstream q, out;
    q << "select name from tag join " << table << "_tags as x on x.tag = id where x."
      << table << " = " << id;
    nanodbc::statement list( conn, q.str() );

    bool first = true;
    auto r = list.execute();

    while ( r.next() )
    {
        out << ( first ? "[ " : ", " ) << r.get< std::string >( 0 );
        first = false;
    }
    out << ( first ? "[]" : " ]" );
    return out.str();
}

template< typename F >
void Report::list_instance( F header, nanodbc::result i )
{
    if ( !i.get< int >( 6 ) )
        return;
    header();

    std::cout << "    - instance id: " << i.get< int >( 0 ) << std::endl
              << "      solver: " << i.get< std::string >( 2 ) << std::endl
              << "      cc opts: ";

    std::stringstream q;
    q << "select opt from cc_opt where config = ?";
    nanodbc::statement ccopt( _conn, q.str() );
    int config = i.get< int >( 7 );
    ccopt.bind( 0, &config );
    auto r = ccopt.execute();
    int count = 0;
    while ( r.next() )
        std::cout << ( count++ ? ", " : "[ " ) << r.get< std::string >( 0 );
    std::cout << ( count ? " ]" : "[]" ) << std::endl;

    std::cout << "      resources: {";
    bool comma = false;
    if ( !i.is_null( 3 ) )
        std::cout << " threads: " << i.get< int >( 3 ), comma = true;
    if ( !i.is_null( 4 ) )
        std::cout << ( comma ? ", " : " " )
                  << "mem: " << i.get< int64_t >( 4 ), comma = true;
    if ( !i.is_null( 5 ) )
        std::cout << ( comma ? ", " : " " )
                  << "time: " << i.get< int64_t >( 5 );
    std::cout << ( comma ? " " : "" ) << "}" << std::endl
              << "      jobs: " << i.get< int >( 6 ) << std::endl
              << "      machine tags: " << list_tags( _conn, "machine", i.get< int >( 1 ) ) << std::endl;
}

void Report::list_build( nanodbc::result r )
{
    int id = r.get< int >( 0 );

    std::stringstream q;
    q << "select note from build_notes where build = " << id;
    nanodbc::statement q_notes( _conn, q.str() );
    auto notes = q_notes.execute();

    bool printed = false;
    auto header = [&]
    {
        if ( printed ) return;
        printed = true;

        std::cout << "- build id: " << id << std::endl
                  << "  version: " << r.get< std::string >( 1 ) << std::endl
                  << "  tags: " << list_tags( _conn, "build", id ) << std::endl;

        if ( notes.next() )
        {
            std::cout << "  note:";
            bool multi = notes.next();
            if ( multi ) std::cout << std::endl;
            notes.first();
            do
                std::cout << ( multi ? "    - " : " " ) << notes.get< std::string >( 0 ) <<std::endl;
            while ( notes.next() );
        }

        std::cout << "  instances:" << std::endl;
    };

    q.str( "" );
    q << "select instance.id, instance.machine, "
      << "config.solver, config.threads, config.max_mem, config.max_time, "
      << "(select count(*) from job where job.instance = instance.id and job.status = 'D'), config.id "
      << "from instance join config on instance.config = config.id where instance.build = ?";
    nanodbc::statement instances( _conn, q.str() );
    instances.bind( 0, &id );
    auto i = instances.execute();
    while ( i.next() )
        list_instance( header, i );
}

void Report::list_instances()
{
    std::stringstream q;
    q << "select id, version from build";
    if ( _tag.size() )
        q << " where ( select count(*) from build_tags as bt join tag on bt.tag = tag.id "
          << "         where tag.name = ? and bt.build = build.id ) > 0";

    nanodbc::statement find( _conn, q.str() );
    if ( _tag.size() )
        find.bind( 0, _tag[ 0 ].c_str() );

    auto r = find.execute();
    while ( r.next() )
        list_build( r );
}

std::unique_ptr< ReportFormat > ReportBase::make_report() const noexcept
{
    if ( _format == "markdown" )
        return std::make_unique< Markdown >();
    else
        UNREACHABLE( "unknown report format: " + _format );
}

void Report::results()
{
    using Type = ReportFormat::Field::Type;

    find_instances();

    if ( _instance_ids.size() != 1 )
        throw brick::except::Error( "Need exactly one instance, but " +
                                    brick::string::fmt( _instance_ids.size() ) + " matched" );

    if ( _watch )
        std::cout << char( 27 ) << "[2J" << char( 27 ) << "[;H";

    std::stringstream q;
    q << "select instid as instance, "
      << ( _by_tag ? "name as tag, " : "name as model, " )
      << ( _by_tag ? "count( modid ) as models, " : "coalesce(variant, '') as variant, result, " )
      << ( _by_tag ? "sum( states ) as states" : "states" ) << ", "
      << ( _by_tag ? "sum(time_search) as search, sum(time_ce) as ce, "
                   : "time_search as search, time_ce as ce, " )
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

    auto report = make_report();

    if ( _by_tag ) {
        report->add_fields( { { "instance", Type::string  }
                            , { "tag"     , Type::string }
                            , { "models"  , Type::integer }
                            , { "states"  , Type::integer }
                            , { "search"  , Type::time }
                            , { "ce"      , Type::time }
                            , { "wrong"   , Type::string } } );
    } else {
        report->add_fields( { { "instance", Type::string }
                            , { "model"   , Type::string }
                            , { "variant" , Type::string }
                            , { "result"  , Type::string }
                            , { "states"  , Type::integer }
                            , { "search"  , Type::time }
                            , { "ce"      , Type::time }
                            , { "correct" , Type::string } } );
    }

    report->from_sql( find.execute() );
    report->format( std::cout );

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
        _fields = { { "time_search", "search" }, { "states", "states" } };
    if ( _instance_ids.empty() )
        throw brick::except::Error( "At least one --instance must be specified." );

    std::stringstream q;
    std::string x0 = "x" + std::to_string( _instance_ids[ 0 ] );

    if ( _by_tag )
        q << "select tag.name as tag, count(" << x0 << ".modid) as models";
    else
        q << "select " << x0 << ".modname as model, " << x0 << ".modvar as variant";

    for ( const auto& [f, as] : _fields )
        for ( auto i : _instance_ids )
            q << ", " << ( _by_tag ? "sum" : "" ) << "(x" << i << "." << as << ") as " << as << "_" << i;
    q << " from ";

    for ( auto it = _instance_ids.begin(); it != _instance_ids.end(); ++it )
    {
        q << "(select model.id as modid, coalesce(model.variant, '') as modvar, model.name as modname";
        for ( const auto& [f, as] : _fields )
            q << ", " << _agg << "(" << f << ") as " << as << " ";
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

    nanodbc::statement find( _conn, q.str() );
    for ( int i = 0; i < int( _instance_ids.size() ); ++i )
        find.bind( i, &_instance_ids[ i ] );
    for ( unsigned i = 0; i < _tag.size(); ++i )
        find.bind( _instance_ids.size() + i, _tag[ i ].c_str() );

    using Field = ReportFormat::Field;
    using Type = Field::Type;

    auto report = make_report();

    if ( _by_tag ) {
        report->add_fields( { { "tag", Type::string }
                            , { "models", Type::integer } } );
    } else {
        report->add_fields( { { "model", Type::string }
                            , { "variant", Type::string } } );
    }

    for ( const auto & [name, as] : _fields ) {
        for ( auto i : _instance_ids ) {
            auto field = as + "_" + std::to_string( i );
            if ( brick::string::startsWith( name, "time" ) )
                report->add_field( { field, Type::time } );
            else
                report->add_field( { field, Type::integer } );
        }
    }

    report->from_sql( find.execute() );
    report->format( std::cout );
}

}
