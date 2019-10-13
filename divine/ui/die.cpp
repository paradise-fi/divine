// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2019 Petr Ročkai <code@fixp.eu>
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

#include <divine/ui/die.hpp>
#include <brick-assert>

#if OPT_Z3
#include <z3++.h>
#endif

#ifdef __unix__
#include <signal.h>
#endif

namespace divine::ui
{
    static ui_wptr _global_ui;

#ifdef __unix__
    void handler( int s )
    {
        signal( s, SIG_DFL );
        if ( auto ui = _global_ui.lock() )
            ui->signal( s );
        raise( s );
    }

    void setup_signals()
    {
        for ( int i = 0; i <= 32; ++i )
            if ( i == SIGCHLD || i == SIGWINCH || i == SIGURG || i == SIGTSTP || i == SIGCONT ||
                 i == SIGKILL )
                continue;
            else
                signal( i, handler );
    }
#else
    void setup_signals() {}
#endif

    void panic()
    {
        try
        {
            if ( std::current_exception() )
                std::rethrow_exception( std::current_exception() );
            std::cerr << "E: std::terminate() called without an active exception" << std::endl;
            std::abort();
        }
        catch ( brq::error &e )
        {
            if ( auto ui = _global_ui.lock() )
                ui->exception( e );
            std::cerr << "E: " << e.what() << std::endl;
            exit( 1 );
        }
        catch ( std::exception &e )
        {
            if ( auto ui = _global_ui.lock() )
                ui->exception( e );
            std::cerr << "E: " << e.what() << std::endl;
            std::abort();
        }
        catch ( brq::assert_failed &e )
        {
            std::cerr << e.what() << std::endl;
            std::abort();
        }
#if OPT_Z3
        catch ( z3::exception &e )
        {
            std::cerr << "E: " << e.msg() << std::endl;
            std::abort();
        }
#endif
        catch ( ... ) {
            std::cerr << "E: unknown exception" << std::endl;
            std::abort();
        }
    }

    void setup_death( ui_wptr ui )
    {
        _global_ui = ui;
        setup_signals();
        std::set_terminate( panic );
    }
}

