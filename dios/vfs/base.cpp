// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2018 Petr Ročkai <code@fixp.eu>
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

#include <dios/vfs/base.hpp>
#include <sys/vmutil.h>

namespace __dios::fs
{
    bool check_key( const _VM_Env *env, const char *suffix )
    {
        const auto npos = std::string_view::npos;
        std::string_view key( env->key );

        if ( key.substr( 0, 4 ) != "vfs." )
            return false;

        if ( key.find( suffix ) == npos )
            return __dios_trace_f( "unexpected vfs key %s", env->key ), false;

        return true;
    }

    bool Base::import( const _VM_Env *env, Map< ino_t, Node > &nodes )
    {
        if ( !env->key )
            return true;

        if ( !check_key( env, ".name" ) )
            return import( env + 1, nodes );

        std::string_view name( env->value, env->size );
        ++ env;

        if ( !check_key( env, ".stat" ) )
            return false;

        const auto &st = *reinterpret_cast< const __vmutil_stat * >( env->value );
        ++env;

        if ( !check_key( env, ".content" ) )
            return false;

        std::string_view content( env->value, env->size );
        auto ino = nodes.count( st.st_ino ) ? nodes[ st.st_ino ] : new_node( st.st_mode, false );
        nodes[ st.st_ino ] = ino;

        if ( auto sym = ino->as< SymLink >() )
            sym->target( content );

        if ( auto reg = ino->as< RegularFile >() )
            reg->content( content );

        link_node( root(), name, ino );
        return import( env + 1, nodes );
    }
}
