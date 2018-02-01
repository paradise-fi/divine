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

#include <tools/extbench.hpp>

namespace cmd  = brick::cmd;
using namespace extbench;

int main( int argc, const char **argv )
{
    auto validator = cmd::make_validator();
    auto args = cmd::from_argv( argc, argv );

    auto helpopts = cmd::make_option_set< benchmark::Help >( validator )
        .option( "[{string}]", &benchmark::Help::_cmd, "print man to specified command"s );

    auto opts_db = cmd::make_option_set< benchmark::Cmd >( validator )
        .option( "[-d {string}]", &benchmark::Cmd::_odbc,
                 "ODBC connection string (default: $DIVBENCH_DB)" );

    auto opts_inst = cmd::make_option_set< benchmark::GetInstance >( validator )
        .option( "[--config-id {int}]", &benchmark::GetInstance::_config_id , "numeric config id" );

    auto opts_job = cmd::make_option_set< benchmark::WithModel >( validator )
        .option( "[--tag {string}]", &benchmark::WithModel::_tag, "only take models with a given tag" );

    auto opts_sched = cmd::make_option_set< benchmark::Schedule >( validator )
        .option( "[--once]", &benchmark::Schedule::_once, "only schedule unique jobs" );

    auto opts_run = cmd::make_option_set< benchmark::Run >( validator )
        .option( "[--single]", &benchmark::Run::_single, "run only single benchmark" );

    auto opts_external = cmd::make_option_set< External >( validator )
        .option( "--driver {string}", &External::_driver, "external divcheck driver" );

    auto cmds = cmd::make_parser( validator )
        .command< Schedule >( opts_db, opts_external, opts_job, opts_inst )
        .command< Run >( opts_db, opts_external, opts_run, opts_job, opts_inst )
        .command< benchmark::Help >( helpopts );
    auto cmd = cmds.parse( args.begin(), args.end() );

    if ( cmd.empty()  )
    {
        benchmark::Help().run( cmds );
        return 0;
    }

    cmd.match( [&]( benchmark::Help &help ) { help.run( cmds ); },
               [&]( benchmark::Cmd &cmd ) { cmd.setup(); cmd.run(); } );
    return 0;
}
