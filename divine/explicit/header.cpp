// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>

// no ifdef O_EXPLICIT here! this must be alwais available (for definitions.h)

#include <divine/explicit/header.h>
#include <divine/graph/probability.h>
#include <fstream>

namespace divine {
namespace dess {

static inline void die( std::string &&msg ) {
    std::cout << msg << std::endl;
    std::abort();
}

static std::string showCapability( Capability c ) {
#define SHOW_CAPABILITY( C ) if ( c == Capability::C ) return #C;
    SHOW_CAPABILITY( ForwardEdges );
    SHOW_CAPABILITY( BackwardEdges );
    SHOW_CAPABILITY( Nodes );
    SHOW_CAPABILITY( UInt64Labels );
    SHOW_CAPABILITY( Probability );
#undef SHOW_CAPABILITY

    return "<<UNKNOWN=" + std::to_string( uint64_t( c ) ) + ">>";
}

std::string to_string( Capabilities c ) {
    std::stringstream ss;
    for ( uint64_t mask = 1; mask; mask <<= 1 ) {
        if ( ( c & mask ) == mask )
            ss << " | " << showCapability( static_cast< Capability >( mask ) );
    }
    ss << " )";
    auto str = ss.str();
    str[ 1 ] = '(';
    return str.substr( 1 );
}

Header *Header::ptr( void *p ) {
    Header *ptr = static_cast< Header * >( p );
    if ( std::memcmp( ptr->magic, MAGIC, MAGIC_LENGTH ) != 0 ) {
        std::cerr << "ERROR: Invalid DIVINE Explicit format" << std::endl;
        std::abort();
    }
    if ( ptr->byteOrder != EXPECTED_BYTE_ORDER ) {
        std::cerr << "ERROR: Invalid byte order (expected 0x" << std::hex
            << EXPECTED_BYTE_ORDER << ", got 0x" << ptr->byteOrder << ")"
            << std::endl;
        std::abort();
    }
    ASSERT_LEQ( 1, ptr->compactVersion );
    if ( ptr->compactVersion > CURRENT_DCESS_VERSION )
        std::cerr << "WARNING: DCESS file was created by newer version of "
            "DIVINE\n and might not be compatible with this version";
    if ( ptr->capabilities.has( Capability::UInt64Labels ) &&
            ptr->labelSize != sizeof( uint64_t ) )
        die( "ERROR: invalid size of saved labels" );
    if ( ptr->capabilities.has( Capability::Probability ) &&
            ptr->labelSize != sizeof( graph::Probability ) )
        die( "Error: invalid size of saved probability labels. ["
                + std::to_string( ptr->labelSize ) + "] != ["
                + std::to_string( sizeof( graph::Probability ) ) + "]" );
    return ptr;
}

Header Header::fromFile( std::string filename ) {
    std::ifstream str( filename, std::ios::in | std::ios::binary );
    union U {
        char chr[ sizeof( Header ) ];
        Header head;
        U() : head() { }
    } u;
    str.read( u.chr, sizeof( Header ) );
    ASSERT( !str.eof() );
    return *ptr( &u.head ); // to check validity
}

}
}
