// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
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

#include <divine/ui/cli.hpp>
#include <divine/ui/logo.hpp>
#include <utility>
#include <string>
#include <sstream>

extern const char *DIVINE_VERSION;
extern const char *DIVINE_SOURCE_SHA;
extern const char *DIVINE_RUNTIME_SHA;
extern const char *DIVINE_BUILD_DATE;
extern const char *DIVINE_BUILD_TYPE;
extern const char *DIVINE_RELEASE_SHA;

using namespace std::literals;

namespace divine {
namespace ui {

std::string version()
{
    std::string composite = DIVINE_SOURCE_SHA + " "s + DIVINE_RUNTIME_SHA;
    if ( DIVINE_RELEASE_SHA == composite )
        return DIVINE_VERSION;
    else {
        std::stringstream v;
        v << DIVINE_VERSION << "+";
        std::string a = std::string( DIVINE_SOURCE_SHA ).substr( 0, 12 ),
                    b = std::string( DIVINE_RUNTIME_SHA ).substr( 0, 12 );
        v << std::hex << (std::stoll( a, 0, 16 ) ^ std::stoll( b, 0, 16 ));
        return v.str();
    }
}

void version::run()
{
    std::cerr << logo << std::endl
              << "DIVINE 4, version " << ui::version() << std::endl << std::endl;

    using P = std::pair< std::string, std::string >;
    for ( auto p : { P{ "version", ui::version() },
                     P{ "source sha", DIVINE_SOURCE_SHA },
                     P{ "runtime sha", DIVINE_RUNTIME_SHA },
                     P{ "build date", DIVINE_BUILD_DATE },
                     P{ "build type", DIVINE_BUILD_TYPE } } )
        std::cout << p.first << ": " << p.second << std::endl;
}

}
}
