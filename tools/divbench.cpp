// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2017,2018 Petr Ročkai <code@fixp.eu>
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

namespace cmd  = brick::cmd;
using namespace benchmark;

int main( int argc, const char **argv )
{

    auto list = []( std::string s, auto good, auto )
    {
        std::vector< std::string > out;
        for ( auto x : brick::string::splitStringBy( s, "," ) )
            out.emplace_back( x );
        return good( out );
    };

    auto validator = cmd::make_validator()->add( "list", list );
    auto args = cmd::from_argv( argc, argv );

    auto helpopts = cmd::make_option_set< Help >( validator )
        .option( "[{string}]", &Help::_cmd, "print man to specified command"s );

    auto opts_db = cmd::make_option_set< Cmd >( validator )
        .option( "[-d {string}]", &Cmd::_odbc, "ODBC connection string (default: $DIVBENCH_DB)" );

    auto opts_inst = cmd::make_option_set< GetInstance >( validator )
        .option( "[--config-id {int}]", &GetInstance::_config_id , "numeric config id" );

    auto opts_report_base = cmd::make_option_set< ReportBase >( validator )
        .option( "[--by-tag]",  &ReportBase::_by_tag, "group results by tags" )
        .option( "[--tag {string}]",  &ReportBase::_tag, "filter results by tag(s)" )
        .option( "[--aggregate {string}]",  &ReportBase::_agg, "run aggregation (default: avg)" )
        .option( "[--watch]",  &ReportBase::_watch, "refresh the results in a loop" )
        .option( "[--instance {list}|--instances {list}]", &ReportBase::_instances, "instance tags" )
        .option( "[--instance-id {int}]", &ReportBase::_instance_ids, "numeric instance id" )
        .option( "[--result {string}]", &ReportBase::_result,
                 "only include runs with one of given results (default: VE)" );

    auto opts_report = cmd::make_option_set< Report >( validator )
        .option( "[--list-instances]",  &Report::_list_instances, "show available instances" );

    auto opts_compare = cmd::make_option_set< Compare >( validator )
        .option( "[--field {string}]",  &Compare::_fields, "include a field in the comparison" );

    auto opts_job = cmd::make_option_set< WithModel >( validator )
        .option( "[--tag {string}]", &WithModel::_tag, "only take models with a given tag" );

    auto opts_setup = cmd::make_option_set< Setup >( validator )
        .option( "[--note {string}]", &Setup::_note, "attach a note to this build" )
        .option( "[--tag {string}]", &Setup::_tag, "attach a tag to this build" );

    auto opts_sched = cmd::make_option_set< Schedule >( validator )
        .option( "[--once]", &Schedule::_once, "only schedule unique jobs" );

    auto opts_run = cmd::make_option_set< Run >( validator )
        .option( "[--single]", &Run::_single, "run only single benchmark" );

    auto cmds = cmd::make_parser( validator )
        .command< Import >( opts_db )
        .command< Setup >( opts_db, opts_setup )
        .command< Schedule >( opts_db, opts_job, opts_sched, opts_inst )
        .command< Report >( opts_db, opts_report_base, opts_report )
        .command< Compare >( opts_db, opts_report_base, opts_compare )
        .command< Run >( opts_db, opts_run, opts_job, opts_inst )
        .command< Help >( helpopts );

    auto cmd = cmds.parse( args.begin(), args.end() );

    if ( cmd.empty()  )
    {
        Help().run( cmds );
        return 0;
    }

    cmd.match( [&]( Help &help ) { help.run( cmds ); },
               [&]( Cmd &cmd ) { cmd.setup(); cmd.run(); } );
    return 0;
}
