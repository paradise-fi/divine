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

#pragma once
#include <brick-fs>
#include <brick-string>

namespace divine::cc
{

const std::string includeDir = "/dios/include";
const std::string srcDir = "/dios/src";

static std::string directory( std::string name )
{
    using brick::fs::joinPath;
    using brick::string::endsWith;
    using brick::string::startsWith;

    if ( startsWith( name, "include/" ) )
        return "/dios";
    for ( auto suffix : { ".c", ".cpp", ".cc" } )
        if ( endsWith( name, suffix ) )
            return srcDir;
    if ( endsWith( name, ".bc" ) || endsWith( name, ".a" ) )
        return "/dios/lib";
    if ( startsWith( name, "sys/" ) || startsWith( name, "macro/" ) )
        return includeDir + "/dios";
    return includeDir;
}

}
