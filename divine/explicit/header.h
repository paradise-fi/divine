// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>

#include <cstdint>
#include <cstring>
#include <string>
#include <memory>

#include <brick-types.h>

#ifndef DIVINE_EXPLICIT_HEADER_H
#define DIVINE_EXPLICIT_HEADER_H

namespace divine {
namespace dess {

constexpr const char *extension = ".dess";

enum class Capability : uint64_t {
    ForwardEdges = 0x1,
    BackwardEdges = 0x2,
    Nodes = 0x4,
    UInt64Labels = 0x100,
    Probability = 0x200

    /* when adding don't forget to update showCapability in header.cpp ! */
};

using Capabilities = brick::types::StrongEnumFlags< Capability >;
std::string to_string( Capabilities );

static const size_t MAGIC_LENGTH = 40UL;
static const char MAGIC[ MAGIC_LENGTH ] = "DIVINE COMPACT EXPLICIT STATE SPACE";
static const int64_t CURRENT_DCESS_VERSION = 1;
static const uint64_t EXPECTED_BYTE_ORDER = 0x0807060504030201ULL;
static const size_t GENERATOR_FIELD_LENGTH = 24UL;

static inline void die( std::string &&msg ) {
    std::cout << msg << std::endl;
    std::abort();
}

struct Header {
    // some meta about divine & compact
    char magic[ MAGIC_LENGTH ];                     //  40B
    uint64_t byteOrder;                             //  48B
    int32_t compactVersion;
    int32_t labelSize;                              //  56B

    static_assert( sizeof( Capabilities ) == 8, "Unexpected size of Capabilities" );
    // informations aubout content of file
    Capabilities capabilities;                      //  64B
    // generator used for nodes (only if nodes are available)
    char generator[ GENERATOR_FIELD_LENGTH ];       //  88B
    int64_t nodeCount;                              //  96B
    // offset to data, from beginning of file
    int64_t dataStartOffset;                        // 104B
    // offset to forward edges, from dataStartOffset
    int64_t forwardOffset;                          // 112B
    int64_t backwardOffset;                         // 120B
    int64_t nodesOffset;                            // 128B

    Header() :
        byteOrder( EXPECTED_BYTE_ORDER ),
        compactVersion( CURRENT_DCESS_VERSION ),
        nodeCount( 0 ),
        dataStartOffset( sizeof( Header ) ),
        forwardOffset( 0 ), backwardOffset( 0 ), nodesOffset( 0 )

    {
        std::copy( MAGIC, MAGIC + MAGIC_LENGTH, magic );
        std::memset( generator, 0, GENERATOR_FIELD_LENGTH );
    }

    static Header *ptr( void *p );
    static Header fromFile( std::string filename );
};

}
}

#endif // DIVINE_EXPLICIT_HEADER_H
