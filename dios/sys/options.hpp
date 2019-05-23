// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016 Jan Mrázek  <email@honzamrazek.cz>
 *     2019 Petr Ročkai <code@fixp.eu>
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

#include <utility>

#include <dios.h>
#include <dios/sys/kernel.hpp>

/* Utilities to deal with the environment (which is passed down from the
 * verification tool and contains options for DiOS itself, command-line
 * arguments, the UNIX environment and a filesystem snapshot). This file deals
 * with everything other than the vfs snapshot. */

namespace __dios
{
    bool parse_sys_options( const _VM_Env *e, SysOpts& res );

    std::string_view extract_opt( std::string_view key, SysOpts& opts );
    bool extract_opt( std::string_view key, std::string_view value, SysOpts& opts );

    /* Construct a null-terminated string from env->value. */
    char *env_to_string( const _VM_Env *env ) noexcept;

    /* Find an env key by name, nullptr if key does not exist. */
    const _VM_Env *get_env_key( const char* key, const _VM_Env *e ) noexcept;

    /* Construct argv/envp-like arguments from env keys beginning with
     * `prefix`.  If `prepend_name`, the binary name is prepended to the array.
     * Return (argc, argv). */
    std::pair< int, char** > construct_main_arg( const char* prefix, const _VM_Env *env,
                                                 bool prepend_name = false ) noexcept;

    /* Trace arguments constructed by construct_main_arg. */
    void trace_main_arg( int indent, std::string_view name, std::pair< int, char** > arg );

    /* Free argv/envp-like arguments created by construct_main_arg. */
    void free_main_arg( char** argv ) noexcept;

}
