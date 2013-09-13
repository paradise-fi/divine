#ifdef O_EXPLICIT
#include <divine/explicit/explicit.h>

#include <ostream>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>

namespace divine {
namespace dess {

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

std::string Capabilities::string() const {
    std::stringstream ss;
    for ( uint64_t mask = 1; mask; mask <<= 1 ) {
        if ( ( (*this) & mask ) == mask )
            ss << " | " << showCapability( static_cast< Capability >( mask ) );
    }
    ss << " )";
    auto str = ss.str();
    str[ 1 ] = '(';
    return str.substr( 1 );
}

Header *Header::ptr( void *p ) {
    auto ptr = reinterpret_cast< Header * >( p );
    if ( std::memcmp( ptr->magic, MAGIC, MAGIC_LENGTH ) != 0 ) {
        std::cerr << "ERROR: Invalid DiVinE Explicit format" << std::endl;
        std::abort();
    }
    if ( ptr->byteOrder != EXPECTED_BYTE_ORDER ) {
        std::cerr << "ERROR: Invalid byte order (expected 0x" << std::hex
            << EXPECTED_BYTE_ORDER << ", got 0x" << ptr->byteOrder << ")"
            << std::endl;
        std::abort();
    }
    assert_leq( 1, ptr->compactVersion );
    if ( ptr->compactVersion > CURRENT_DCESS_VERSION )
        std::cerr << "WARNING: DCESS file was created by newer version of "
            "DiVinE\n and might not be compatible with this version";
    if ( ptr->capabilities.has( Capability::UInt64Labels ) &&
            ptr->labelSize != sizeof( uint64_t ) )
        die( "ERROR: invalid size of saved labels" );
    if ( ptr->capabilities.has( Capability::Probability ) &&
            ptr->labelSize != sizeof( toolkit::Probability ) )
        die( "Error: invalid size of saved probability labels. ["
                + std::to_string( ptr->labelSize ) + "] != ["
                + std::to_string( sizeof( toolkit::Probability ) ) + "]" );
    return ptr;
}

void Explicit::open( std::string path, OpenMode om ) {
    int openmode = om == OpenMode::Read ? O_RDONLY : O_RDWR;
    int mmapProt = om == OpenMode::Read ? PROT_READ : PROT_READ | PROT_WRITE;

    struct stat st;
    int fd = ::open( path.c_str(), openmode );
    assert_leq( 0, fd );
    auto r = ::fstat( fd, &st );
    assert_eq( r, 0 );
    static_cast< void >( r );
    size_t size = st.st_size;

    void *ptr = ::mmap( nullptr, size, mmapProt, MAP_SHARED, fd, 0 );
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
    assert_neq( ptr, MAP_FAILED );
#pragma GCC diagnostic pop

    finishOpen( fd, ptr, size );
}

void Explicit::finishOpen( int fd, void *ptr, int size ) {
    header = std::shared_ptr< Header >( Header::ptr( ptr ),
            [ fd, size ]( Header *h ) {
                ::munmap( h, size );
                ::close( fd );
            } );

    char *cptr = reinterpret_cast< char* >( ptr );

    assert_leq( header->compactVersion, 1 );
    assert_leq( header->dataStartOffset, int64_t( sizeof( Header ) ) );

    if ( header->capabilities.has( Capability::ForwardEdges ) )
        forward = DataBlock( header->nodeCount,
                cptr + header->dataStartOffset + header->forwardOffset );
    if ( header->capabilities.has( Capability::BackwardEdges ) )
        backward = DataBlock( header->nodeCount,
                cptr + header->dataStartOffset + header->backwardOffset );
    if ( header->capabilities.has( Capability::Nodes ) )
        nodes = DataBlock( header->nodeCount,
                cptr + header->dataStartOffset + header->nodesOffset );
}

PrealocateHelper::PrealocateHelper( const std::string &path ) :
    _edges( 0 ), _nodes( 0 ),
    _nodeDataSize( 0 ), _labelSize( 0 ), _capabilities()
{
    fd = ::open( path.c_str(), O_RDWR | O_CREAT,
        S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
    assert_leq( 0, fd );
}

Explicit PrealocateHelper::operator()() {
    assert_leq( 0, fd );
    assert( ( !_capabilities.has( Capability::Probability )
                && !_capabilities.has( Capability::UInt64Labels ) )
            || ( _capabilities.has( Capability::Probability )
                ^ _capabilities.has( Capability::UInt64Labels ) ) );

    int64_t edgeData = sizeof( int64_t ) * _nodes
        + _edges * (sizeof( int64_t ) + _labelSize);
    assert( !_capabilities.has( Capability::UInt64Labels )
            || _labelSize == sizeof( uint64_t ) );
    assert( !_capabilities.has( Capability::Probability )
            || _labelSize == sizeof( toolkit::Probability ) );

    int64_t nodeData = sizeof( int64_t ) * _nodes + _nodeDataSize;

    int64_t fileSize = sizeof( Header );
    if ( _capabilities.has( Capability::ForwardEdges ) )
        fileSize += edgeData;
    if ( _capabilities.has( Capability::BackwardEdges ) )
        fileSize += edgeData;
    if ( _capabilities.has( Capability::Nodes ) )
        fileSize += nodeData;


    auto r = ::posix_fallocate( fd, 0, fileSize );
    assert_eq( r , 0 );
    static_cast< void >( r );

    void *ptr = ::mmap( nullptr, fileSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
    assert_neq( ptr, MAP_FAILED );
#pragma GCC diagnostic pop

    Header *h = new ( ptr ) Header();
    h->capabilities = _capabilities;
    h->nodeCount = _nodes;

    h->forwardOffset = 0;
    h->backwardOffset = _capabilities.has( Capability::ForwardEdges )
        ? edgeData : 0;
    h->nodesOffset = h->backwardOffset
        + (_capabilities.has( Capability::BackwardEdges ) ? edgeData : 0);
    h->labelSize = _labelSize;

    Explicit compact;
    compact.finishOpen( fd, ptr, fileSize );

    auto end = _generator.end() - _generator.begin() > int( GENERATOR_FIELD_LENGTH )
                ? _generator.begin() + GENERATOR_FIELD_LENGTH
                : _generator.end();
    std::copy( _generator.begin(), end, compact.header->generator );

    fd = -1;
    return compact;
}

}
}
#endif // O_EXPLICIT
