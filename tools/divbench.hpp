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

#pragma once

#include <external/nanodbc/nanodbc.h>
#include <divine/ui/odbc.hpp>
#include <divine/ui/cli.hpp>
#include <divine/ui/log.hpp>
#include <divine/rt/dios-cc.hpp>
#include <brick-except>
#include <brick-fs>

#include <chrono>

namespace benchmark
{

using namespace nanodbc;
using namespace divine;
using namespace std::literals;

namespace fs   = brick::fs;
namespace odbc = divine::ui::odbc;

int get_instance( nanodbc::connection conn, int config, int build = 0 );

struct WithConnection
{
    connection _conn;
    void add_tag( std::string table, int id, std::string tag );
};

struct Cmd : WithConnection
{
    std::string _odbc;
    void setup();
    virtual void run() = 0;
};

struct Help
{
    std::string _cmd = std::string("");

    template< typename P >
    void run( P cmds )
    {
        std::string description = cmds.describe( _cmd );
        if ( description.empty() && !_cmd.empty() )
            ui::die( "Unknown command '" + _cmd + "'. Available commands are:\n" + cmds.describe() );
        if ( _cmd.empty() )
        {
            std::cerr << "To print details about a specific command, run 'divine help {command}'.\n\n";
            std::cerr << cmds.describe() << std::endl;
        }
        else std::cerr << description << std::endl;
    }
};

struct ImportModel : WithConnection
{
    ImportModel( const connection &c ) { _conn = c; }

    std::string _name, _variant, _path, _interp;
    std::vector< std::string > _tags;
    std::set< std::pair< std::string, int > > _file_ids;
    int _id = 0, _revision, _script_id;

    std::string ident( bool = true );
    void import();
    void do_import();
    bool dedup();
    void get_revision();
    void tag();
    int put_file( std::string name, std::string content );
};

struct Import : Cmd
{
    virtual void run();
};

struct WithModel : virtual Cmd
{
    std::string _tag;
};

struct GetInstance : virtual Cmd
{
    int _config_id = 1;
    virtual int get_instance();
};

struct Setup : GetInstance
{
    std::string _tag;
    std::string _note;
    void run() override;
};

struct Schedule : WithModel, virtual GetInstance
{
    bool _once = false;
    void run() override;
};


struct Record
{
    using null_value = std::monostate;
    using Value = std::variant< null_value, int, std::string, std::chrono::milliseconds >;

    std::map< std::string, Value > data;

    void add_member( std::string name, Value value ) {
        data.emplace( name, value );
    }
};


struct ReportFormat
{
    struct Field
    {
        enum class Type { string, integer, time };

        std::string name;
        Type type;
    };

    std::vector< Field > fields;
    std::vector< Record > records;

    void add_field( Field && f ) noexcept
    {
        fields.push_back( std::move( f ) );
    }

    void add_fields( std::vector< Field > && fs ) noexcept
    {
        for ( auto && f : fs )
            add_field( std::move( f ) );
    }

    virtual ~ReportFormat() = default;

    virtual void format( std::ostream &os ) noexcept = 0;
    virtual Record record_from_sql( const nanodbc::result & row ) const noexcept = 0;

    void from_sql( nanodbc::result && res );
};


struct ReportBase : WithModel
{
    bool _by_tag = false, _watch = false;
    std::vector< std::string > _tag;
    std::string _result = "VE";
    std::string _agg = "avg";
    std::string _format = "markdown";
    std::vector< std::vector< std::string > > _instances;
    std::vector< int > _instance_ids;

    std::unique_ptr< ReportFormat > make_report() const noexcept;

    void find_instances();
    void find_instances( std::vector< std::string > );
};

struct Report : ReportBase
{
    bool _list_instances = false;

    void list_build( nanodbc::result );

    template< typename F >
    void list_instance( F header, nanodbc::result i );
    void list_instances();
    void results();

    void run() override
    {
        if ( _list_instances )
            list_instances();
        else
            results();
    }
};

struct CompareBase : ReportBase
{
    using field_map = std::map< std::string, std::string >;
    std::unique_ptr< ReportFormat > get_comparison_report( const field_map &fields );
};

struct Compare : CompareBase
{
    std::map< std::string, std::string > _fields;
    void run() override;
};

struct Plot : CompareBase
{
    std::string _output_file;
    std::string _field = "time_search";
    int _width = 800, _height = 600, _rows = 10;

    void run() override;
};

struct Run : virtual GetInstance, WithModel
{
    std::vector< std::pair< std::string, std::string > > _files;
    std::string _script;
    bool _done = false, _single = false;
    ui::SinkPtr _log;
    int _model;

    void prepare( int model );
    void prepare( rt::DiosCC &drv );

    virtual void execute( int job );
    void log_start( int job, int exec );
    void log_done( int job );

    void config( ui::Cc &cc );
    void config( ui::Verify &verify );

    virtual void run();
};

}
