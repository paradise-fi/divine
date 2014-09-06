// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>

#if ALG_EXPLICIT || GEN_EXPLICIT
#include <divine/explicit/explicit.h>
#include <divine/graph/probability.h>

#include <ostream>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

namespace divine {
namespace dess {

using namespace wibble::sys;
using wibble::operator|;

void Explicit::open( std::string path, OpenMode om ) {
    auto mmapProt = om == OpenMode::Read ? MMap::ProtectMode::Read
        : MMap::ProtectMode::Read | MMap::ProtectMode::Write;

    map.map( path, MMap::ProtectMode::Shared | mmapProt );
    finishOpen();
}

void Explicit::finishOpen() {
    header = Header::ptr( map.asArrayOf< void >() );
    char *cptr = map.asArrayOf< char >();

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
            || _labelSize == sizeof( graph::Probability ) );

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

    MMap map( fd, MMap::ProtectMode::Read | MMap::ProtectMode::Write
            | MMap::ProtectMode::Shared );

    Header *h = new ( map.asArrayOf< void >() ) Header();
    h->capabilities = _capabilities;
    h->nodeCount = _nodes;

    h->forwardOffset = 0;
    h->backwardOffset = _capabilities.has( Capability::ForwardEdges )
        ? edgeData : 0;
    h->nodesOffset = h->backwardOffset
        + (_capabilities.has( Capability::BackwardEdges ) ? edgeData : 0);
    h->labelSize = _labelSize;

    Explicit compact;
    compact.map = map;
    compact.finishOpen();

    auto end = _generator.end() - _generator.begin() > int( GENERATOR_FIELD_LENGTH )
                ? _generator.begin() + GENERATOR_FIELD_LENGTH
                : _generator.end();
    std::copy( _generator.begin(), end, compact.header->generator );

    fd = -1;
    return compact;
}

}
}
#endif
