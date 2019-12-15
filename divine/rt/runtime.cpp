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

#include <divine/rt/runtime.hpp>

namespace divine::str
{
    struct stringtable { std::string n; std::string_view c; };
    extern stringtable dios_list[];
    namespace dios_native { extern std::string_view libdios_host_a, libc__abi_a, libc___a; }
}

namespace divine::rt
{

    static std::string fixname( std::string n )
    {
        if ( n == "libcxx.a" ) n = "libc++.a";
        if ( n == "libcxxabi.a" ) n = "libc++abi.a";
        if ( brq::ends_with( n, ".bc" ) || brq::ends_with( n, ".a" ) )
            return "/dios/lib/" + n;
        else
            return "/dios/" + n;
    }

    void each( std::function< void( std::string, std::string_view ) > yield )
    {
        for ( auto src = str::dios_list; !src->n.empty(); ++src )
        {
            TRACE( "yielding", fixname( src->n ) );
            yield( fixname( src->n ), src->c );
        }
    }

    std::string_view dios_host()
    {
        return divine::str::dios_native::libdios_host_a;
    }

    std::string_view libcxxabi()
    {
        return divine::str::dios_native::libc__abi_a;
    }

    std::string_view libcxx()
    {
        return divine::str::dios_native::libc___a;
    }

}
