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
    std::string _pkg, _name;
    std::vector< std::string > _srcs, _hdrs;

    int modrev();
    void files();
    void pkg() { NOT_IMPLEMENTED(); }

    virtual void run()
    {
        if ( !_srcs.empty() && !_pkg.empty() )
            throw brick::except::Error( "--src and --pkg can't be both specified" );

        if ( _pkg.empty() )
            files();
        else
            pkg();
    }
};

struct Schedule : Cmd
{
    void run() override;
};

struct Report : Cmd
{
    bool _list_instances = false;
    int _instance = -1;

    void format( nanodbc::result res, odbc::Keys );
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

struct Run : Cmd
{
    std::vector< std::pair< std::string, std::string > > _files;
    std::vector< std::string > _script;
    bool _done;

    void prepare( int model );

    void execute( int job );
    void execute( int, ui::Verify & );
    void execute( int, ui::Run & ) { NOT_IMPLEMENTED(); }

    virtual void run();
};

}
