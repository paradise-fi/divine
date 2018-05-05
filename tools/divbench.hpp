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
#include <divine/cc/compile.hpp>
#include <divine/ui/odbc.hpp>
#include <divine/ui/cli.hpp>
#include <divine/ui/log.hpp>
#include <brick-except>
#include <brick-fs>

namespace benchmark
{

using namespace nanodbc;
using namespace divine;
using namespace std::literals;

namespace fs   = brick::fs;
namespace odbc = divine::ui::odbc;

int get_instance( nanodbc::connection conn, int config, int build = 0 );

struct Cmd
{
    std::string _odbc;
    connection  _conn;
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
            die( "Unknown command '" + _cmd + "'. Available commands are:\n" + cmds.describe() );
        if ( _cmd.empty() )
        {
            std::cerr << "To print details about a specific command, run 'divine help {command}'.\n\n";
            std::cerr << cmds.describe() << std::endl;
        }
        else std::cerr << description << std::endl;
    }
};

struct ImportModel
{
    connection &_conn;
    ImportModel( connection &c ) : _conn( c ) {}

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

struct Schedule : WithModel, virtual GetInstance
{
    bool _once = false;
    void run() override;
};

struct ReportBase : WithModel
{
    bool _by_tag = false, _watch = false;
    std::vector< std::string > _tag;
    std::string _result = "VE";
    std::string _agg = "avg";
    std::vector< std::vector< std::string > > _instances;
    std::vector< int > _instance_ids;
    void find_instances();
    void find_instances( std::vector< std::string > );
};

struct Report : ReportBase
{
    bool _list_instances = false;

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

struct Compare : ReportBase
{
    std::vector< std::string > _fields;
    void run() override;
};

struct Run : virtual GetInstance, WithModel
{
    std::vector< std::pair< std::string, std::string > > _files;
    std::string _script;
    bool _done = false, _single = false;
    ui::SinkPtr _log;

    void prepare( int model );

    virtual void execute( int job );
    void log_start( int job, int exec );
    void log_done( int job );

    void config( ui::Cc &cc );
    void config( ui::Verify &verify );

    virtual void run();
};

}
