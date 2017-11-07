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

struct Cmd
{
    std::string _odbc;
    connection  _conn;
    void setup();
    virtual void run() = 0;
};

struct Import : Cmd
{
    std::string _name, _script, _variant;
    std::vector< std::string > _files, _tags;
    int _id;

    int modrev();
    bool files();
    void tag();

    virtual void run()
    {
        if ( files() )
            tag();
    }
};

struct JobBase : Cmd
{
    std::string _tag;
};

struct External : virtual odbc::BuildID
{
    std::string _driver;

    int get_build( nanodbc::connection ) override;
};

struct Schedule : JobBase, virtual odbc::BuildID
{
    void run() override;
};

struct ScheduleExternal : Schedule, External
{ };

struct ReportBase : Cmd
{
    bool _by_tag = false, _watch = false;
    std::string _result = "VE";
    std::string _agg = "avg";
};

struct Report : ReportBase, odbc::BuildID
{
    bool _list_instances = false;
    int _instance = -1;

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
    std::vector< int > _instances;
    std::vector< std::string > _fields;
    void run() override;
};

struct Run : JobBase, virtual odbc::BuildID
{
    std::vector< std::pair< std::string, std::string > > _files;
    std::string _script;
    bool _done = false, _single = false;
    ui::SinkPtr _log;

    void prepare( int model );
    int get_instance();

    virtual void execute( int job );
    void log_start( int job );
    void log_done( int job );

    virtual void run();
};

struct RunExternal : Run, External
{
    void execute( int job ) override;
};

}
