// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2019 Zuzana Baranová <xbaranov@fi.muni.cz>
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

#include <divine/cc/codegen.hpp>
#include <divine/cc/link.hpp>
#include <divine/cc/options.hpp>

#include <brick-proc>

namespace divine::cc
{
    int compileFiles( cc::ParsedOpts& po, cc::CC1& clang, PairedFiles& objFiles )
    {
        for ( auto file : objFiles )
        {
            if ( file.first == "lib" )
                continue;
            if ( cc::is_object_type( file.first ) )
                continue;
            auto mod = clang.compile( file.first, po.opts );
            cc::emit_obj_file( *mod, file.second );
        }
        return 0;
    }
}
