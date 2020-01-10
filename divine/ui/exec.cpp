// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016 Petr Ročkai <code@fixp.eu>
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

#include <divine/mc/exec.hpp>
#include <divine/ui/cli.hpp>

namespace divine::ui
{
    void exec::setup()
    {
        if ( _report != arg::report::yaml )
        {
            _report = arg::report::yaml;
            std::cerr << "W: exec doesn't print log to the stdout" << std::endl;
        }

        if ( _log == nullsink() && _do_report_file )
        {
            setup_report_file();
            _report_file.reset( new std::ofstream( _report_filename.name ) );

             // TODO: We don't want interactive, but we want booting messages?
            _log = make_composite( { make_interactive(), make_yaml( *_report_file.get(), true ) } );
        }

        if ( _bc_opts.symbolic && !_virtual )
        {
            _virtual = true;
            std::cerr << "W: --symbolic implies --virtual" << std::endl;
        }

        if ( _bc_opts.dios_config.empty() )
            _bc_opts.dios_config = _virtual ? "default" : "proxy";

        if ( _tactic == "coverage" )
            _bc_opts.lart_passes.push_back( "coverage" );

        with_bc::setup();
    }

    void exec::run()
    {
        mc::Exec exec( bitcode() );

        _log->start();

        if ( _trace )
            exec.trace();
        else
            exec.run( _exhaustive, _tactic ); // TODO: What about trace?

        _log->progress( { 0, 0 }, 0, true );
        _log->memory( exec.poolstats(), mc::HashStats(), true ); // TODO: What about HashStats?

        report_options();

        _log->result( mc::Result::None, mc::Trace() );
    }
}
