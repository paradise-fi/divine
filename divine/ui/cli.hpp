// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016 Petr Ročkai <code@fixp.eu>
 * (c) 2016 Viktória Vozárová <>
 * (c) 2016 Vladimír Štill <xstill@fi.muni.cz>
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

#include <divine/mc/bitcode.hpp>
#include <divine/mc/job.hpp>
#include <divine/cc/options.hpp>
#include <divine/rt/dios-cc.hpp>
#include <divine/sim/trace.hpp>

#include <divine/ui/common.hpp>
#include <divine/ui/curses.hpp>
#include <divine/ui/die.hpp>
#include <divine/ui/log.hpp>

#include <brick-cmd>
#include <brick-fs>
#include <brick-string>
#include <cctype>
#include <regex>

#include <divine/vm/divm.h>

namespace divine::ui::arg
{
    struct mem
    {
        size_t size;
        mem( size_t s = 0 ) : size( s ) {}
    };

    struct mount
    {
        std::string capture;
        std::string mount;
        bool followSymlink;
    };

    template< typename type >
    struct commasep
    {
        std::vector< type > vec;
    };

    enum class report { none, yaml, yaml_long };

    static brq::parse_result from_string( std::string_view s, mem &m )
    {
        size_t pos;
        m.size = stoull( std::string( s ), &pos );
        auto r = s.substr( pos );

        if ( r.empty() )
            return {};

        std::string rem;
        std::transform( r.begin(), r.end(), std::back_inserter( rem ), ::tolower );
        if ( rem == "k" || rem == "kb" )
            m.size *= 1000;
        else if ( rem == "ki" || rem == "kib" )
            m.size *= 1024;
        else if ( rem == "m" || rem == "mb" )
            m.size *= 1000'000;
        else if ( rem == "mi" || rem == "mib" )
            m.size *= 1024 * 1024;
        else if ( rem == "g" || rem == "gb" )
            m.size *= 1'000'000'000;
        else if ( rem == "gi" || rem == "gib" )
            m.size *= 1024 * 1024 * 1024;
        else
            return brq::no_parse( "unknown memory size suffix", r );

        return {};
    }

    static brq::parse_result from_string( std::string_view s, mount &m )
    {
        m.followSymlink = true;
        std::regex sep( ":" );
        std::regex_token_iterator it( s.begin(), s.end(), sep, -1 );
        int i;

        for ( i = 0; it != decltype( it )(); it++, i++ )
            switch ( i )
            {
                case 0:
                    m.capture = *it;
                    m.mount = *it;
                    break;
                case 1:
                    if ( *it == "follow" )
                        m.followSymlink = true;
                    else if ( *it == "nofollow" )
                        m.followSymlink = false;
                    else
                        return brq::no_parse( "invalid option for follow links in", s );
                    break;
                case 2:
                    m.mount = *it;
                    break;
                default:
                    return brq::no_parse( "unexpected attribute", *it, "in", s );
            }

        if ( i < 1 )
            return brq::no_parse( "the directory to capture is missing in", s );
        if ( !brq::file_access( m.capture, F_OK ) )
            return brq::no_parse( "file or directory", m.capture, "does not exist" );
        if ( !brq::file_access( m.capture, R_OK ) )
            return brq::no_parse( "file or directory", m.capture, "is not readable" );

        return {};
    }

    template< typename T >
    brq::parse_result from_string( std::string_view s, commasep< T > &cs )
    {
        using brq::from_string;
        T val;

        for ( auto c : brq::splitter( s, ',' ) )
        {
            if ( auto r = from_string( c, val ) ; !r )
                return r;
            else
                cs.vec.push_back( val );
        }

        return {};
    }

    static brq::parse_result from_string( std::string_view s, report &r )
    {
        if      ( s == "none" ) r = report::none;
        else if ( s == "yaml" ) r = report::yaml;
        else if ( s == "yaml-long" ) r = report::yaml_long;
        else return brq::no_parse( "report type must be none, yaml or yaml-long" );
        return {};
    }
}

namespace divine::ui
{
    void run() override;
};

enum class Report { None, Yaml, YamlLong };

struct Ltlc : Command
{
    std::string _automaton, _formula, _output, _system;
    bool _negate = false;
    void run() override;
};

struct Verify : WithBC
{
    int64_t _max_mem = 0; // bytes
    int _max_time = 0;  // seconds
    int _threads = 0;
    int _num_callers = 10;
    int _poolstat_period = 0;
    bool _no_counterexample = false;
    bool _interactive = true;
    bool _liveness = false;
    bool _no_report_file = false;
    bool _report_unique = false;
    Report _report = Report::Yaml;
    std::string _solver = "stp";

    std::shared_ptr< std::ostream > _report_file;
    std::string _report_filename;
    void setup_report_file();

    void setup() override;
    void run() override;
    void cleanup() override;

    void safety();
    void liveness();
    void print_ce( mc::Job &job );
};

struct Check : Verify
{
    void setup() override;
};

struct Exec : WithBC
{
    bool _trace = false;
    bool _virtual = false;

    void setup();
    void run();
};

struct Sim : WithBC
{
    bool _batch = false, _skip_init = false, _load_report = false;
    std::shared_ptr< sim::Trace > _trace;

#if OPT_SIM
        void process_options() override;
        void run() override;
#else
        void process_options() {}
        void run()
        {
            throw std::runtime_error( "This build of DIVINE does not support the 'sim' command." );
        }
#endif
    };

    struct draw : with_bc
    {
        int _number = 0;
        int _distance = 32;
        enum { All, None, Trace } _labels = None;
        bool _bfs = false, _raw = false;
        std::string _render = "dot -Tx11";
        std::string _output = "-";

        void run() override;

        void options( brq::cmd_options &c ) override
        {
            with_bc::options( c );
            c.section( "Draw Options" );
            c.opt( "--distance", _distance ) << "maximum distance from the initial state [32]";
            c.opt( "--render", _render ) << "command to pipe the 'dot' output into [dot -Tx11]";
        }
    };

    struct cc : command
    {
        cc() = default;
        cc( cc && ) = default;
        cc& operator=( cc&& ) = default;

        divine::cc::Options _opts;
        std::vector< std::string > _flags;
        std::vector< arg::commasep< std::string > > _passThroughFlags;
        std::string _output;
        rt::DiosCC _driver;

        void run();

        void options( brq::cmd_options &c ) override
        {
            command::options( c );
            c.opt( "-c", _opts.dont_link ) << "compile but do not link";
            c.opt( "--dont-link", _opts.dont_link ) << "alias for the above";
            c.opt( "-o", _output ) << "write the output into a given file";
            c.opt( "-C,", _passThroughFlags ) << "pass additional options to the compiler";
            c.collect( _flags );
        }
    };

    struct info : exec
    {
        info()
        {
            _systemopts.push_back( "help" );
        }

        void run() override;
    };

    static std::string outputName( std::string path, std::string ext )
    {
        return brq::replace_extension( brq::basename( path ), ext );
    }

    std::shared_ptr< Interface > make_cli( int argc, const char **argv );
}
