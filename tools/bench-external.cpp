// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2017 Vladimir Still <xstill@fi.muni.cz>
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

#include <divine/ui/odbc.hpp>

#include <iostream>
#include <fstream>
#include <vector>
#include <brick-string>
#include <brick-types>
#include <brick-fs>
#include <brick-cmd>
#include <brick-proc>
#include <brick-yaml>

namespace benchmark
{

namespace fs = brick::fs;
namespace proc = brick::proc;
namespace ui = divine::ui;
namespace yaml = brick::yaml;
using divine::mc::Result;

char toResult( std::string val )
{
    if ( val == "yes" || val == "Yes" ) return 'E';
    if ( val == "no" || val == "No" ) return 'V';
    return 'U';
}

int External::get_build()
{
    auto r = proc::spawnAndWait( proc::CaptureStdout | proc::ShowCmd, _driver );
    if ( !r )
        throw brick::except::Error( "benchmark driver failed: exitcode " + std::to_string( r.exitcode() )
                                    + ", signal " + std::to_string( r.signal() ) );
    yaml::Parser yinfo( r.out() );

    odbc::ExternalBuildInfo info;
    info.driver = yinfo.getOr< std::string >( { "driver" }, "{unknown driver}" );
    info.driver_checksum = yinfo.getOr< std::string >( { "driver checksum" }, "0" );
    info.checksum = yinfo.getOr< std::string >( { "checksum" }, "0" );
    info.version = yinfo.getOr< std::string >( { "version" }, "0" );
    info.build_type = yinfo.getOr< std::string >( { "build type" }, "unknown" );

    return odbc::get_external_build( _conn, info );
}

int External::get_instance()
{
    return benchmark::get_instance( _conn, get_build() );
}

void RunExternal::execute( int job_id )
{
    fs::TempDir workdir( "_divbench_run_external.XXXXXX",
                         fs::AutoDelete::Yes,
                         fs::UseSystemTemp::Yes );
    fs::ChangeCwd cwd( workdir.path );

    for ( const auto &f : _files ) {
        fs::mkFilePath( f.first );
        std::ofstream file( f.first );
        file << f.second;
    }

    {
        std::ofstream script( "script" );
        script << _script;
    }

    int exec_id = odbc::add_execution( _conn );
    log_start( job_id, exec_id );

    auto r = proc::spawnAndWait( proc::ShowCmd, _driver, "script" );
    if ( !r )
        throw brick::except::Error( "benchmark driver failed: exitcode " + std::to_string( r.exitcode() )
                                    + ", signal " + std::to_string( r.signal() ) );

    std::string report = fs::readFile( "report.yaml" );
    std::cerr << "REPORT: " << std::endl << report << std::endl;

    yaml::Parser yreport( report );

    auto timer = [&]( std::string n ) {  return yreport.getOr< double >( { "timers", n }, 0 ) * 1000; };
    auto states = yreport.getOr< long >( { "state count" }, 0 );
    char result = toResult( yreport.getOr< std::string >( { "error found" }, "null" ) );

    odbc::update_execution( _conn, exec_id, result, timer( "lart" ), timer( "load" ),
                            timer( "boot" ), timer( "search" ), timer( "ce" ), states );
    log_done( job_id );
}

} // namespace benchmark
