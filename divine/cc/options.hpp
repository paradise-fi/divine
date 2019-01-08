#pragma once

#include <divine/cc/filetype.hpp>

namespace divine {
namespace cc {

struct Options
{
    bool dont_link;
    bool verbose;
    Options() : Options( false, true ) {}
    Options( bool dont_link, bool verbose ) : dont_link( dont_link ), verbose( verbose ) {}
};

struct ParsedOpts
{
    std::vector< std::string > opts;
    std::vector< std::string > libSearchPath;
    std::vector< FileEntry > files;
    std::string outputFile;
    std::vector< std::string > allowedPaths;
    bool toObjectOnly = false;
    bool preprocessOnly = false;
    bool hasHelp = false;
    bool hasVersion = false;
};

ParsedOpts parseOpts( std::vector< std::string > rawCCOpts );

}
}
