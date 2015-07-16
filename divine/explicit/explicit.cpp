// -*- C++ -*- (c) 2013-2015 Vladimír Štill <xstill@fi.muni.cz>

#if ALG_EXPLICIT || GEN_EXPLICIT
#include <divine/explicit/explicit.h>
#include <divine/graph/probability.h>
#include <brick-bitlevel.h> // align

#include <ostream>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>


static bool fallocate(int fd, off_t len)
{
#if defined(HAVE_POSIX_FALLOCATE)
  return posix_fallocate(fd, 0, len) == 0;
#elif defined(XP_MACOSX)
  fstore_t store = {F_ALLOCATECONTIG, F_PEOFPOSMODE, 0, aLength};
  // Try to get a continous chunk of disk space
  int ret = fcntl(fd, F_PREALLOCATE, &store);
    if(-1 == ret){
    // OK, perhaps we are too fragmented, allocate non-continuous
    store.fst_flags = F_ALLOCATEALL;
    ret = fcntl(fd, F_PREALLOCATE, &store);
    if (-1 == ret)
      return false;
  }
  return 0 == ftruncate(fd, len);
#endif
  return false;
}


namespace divine {
namespace dess {

using namespace brick::mmap;

void Explicit::open( std::string path, OpenMode om ) {
    auto mmapProt = om == OpenMode::Read ? ProtectMode::Read
        : ProtectMode::Read | ProtectMode::Write;

    map.map( path, ProtectMode::Shared | mmapProt );
    finishOpen();
}

void Explicit::finishOpen() {
    header = Header::ptr( map.asArrayOf< void >() );
    char *cptr = map.asArrayOf< char >();

    ASSERT_LEQ( 1, header->compactVersion );
    ASSERT_LEQ( headerLength[ header->compactVersion ], header->dataStartOffset );
    ASSERT( !header->capabilities.has( Capability::StateFlags )
            || header->flagMaskBitWidth == 64 );

    if ( header->capabilities.has( Capability::ForwardEdges ) )
        forward = DataBlock( header->nodeCount,
                cptr + header->dataStartOffset + header->forwardOffset );
    if ( header->capabilities.has( Capability::BackwardEdges ) )
        backward = DataBlock( header->nodeCount,
                cptr + header->dataStartOffset + header->backwardOffset );
    if ( header->capabilities.has( Capability::Nodes ) )
        nodes = DataBlock( header->nodeCount,
                cptr + header->dataStartOffset + header->nodesOffset );
    if ( header->capabilities.has( Capability::StateFlags ) )
        stateFlags = StateFlags( header->flagCount,
                cptr + header->dataStartOffset + header->flagMapOffset,
                cptr + header->dataStartOffset + header->flagsOffset );
}

PrealocateHelper::PrealocateHelper( const std::string &path ) :
    _edges( 0 ), _nodes( 0 ),
    _nodeDataSize( 0 ), _labelSize( 0 ),
    _flagCount( 0 ), _flagStrings( 0 ), _capabilities()
{
    fd = ::open( path.c_str(), O_RDWR | O_CREAT,
        S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
    ASSERT_LEQ( 0, fd );
}

Explicit PrealocateHelper::operator()() {
    ASSERT_LEQ( 0, fd );
    ASSERT( ( !_capabilities.has( Capability::Probability )
                && !_capabilities.has( Capability::UInt64Labels ) )
            || ( _capabilities.has( Capability::Probability )
                ^ _capabilities.has( Capability::UInt64Labels ) ) );

    int64_t edgeData = sizeof( int64_t ) * _nodes
        + _edges * (sizeof( int64_t ) + _labelSize);
    ASSERT( !_capabilities.has( Capability::UInt64Labels )
            || _labelSize == sizeof( uint64_t ) );
    ASSERT( !_capabilities.has( Capability::Probability )
            || _labelSize == sizeof( graph::Probability ) );

    int64_t nodeData = sizeof( int64_t ) * _nodes +
                    brick::bitlevel::align( _nodeDataSize, sizeof( int64_t ) );
    int64_t flagNames = sizeof( int64_t ) * _flagCount +
                    brick::bitlevel::align( _flagStrings, sizeof( int64_t ) );

    int64_t fileSize = sizeof( Header );
    if ( _capabilities.has( Capability::ForwardEdges ) )
        fileSize += edgeData;
    if ( _capabilities.has( Capability::BackwardEdges ) )
        fileSize += edgeData;
    if ( _capabilities.has( Capability::Nodes ) )
        fileSize += nodeData;
    if ( _capabilities.has( Capability::StateFlags ) ) {
        fileSize += flagNames;
        fileSize += sizeof( uint64_t ) * _nodes; // flags
    }

    auto r = ::ftruncate( fd, 0 );
    ASSERT_EQ( r, 0 );
    r = fallocate( fd, fileSize );
    ASSERT_EQ( r , 0 );
    static_cast< void >( r );

    MMap map( fd, ProtectMode::Read | ProtectMode::Write
            | ProtectMode::Shared );

    Header *h = new ( map.asArrayOf< void >() ) Header();
    h->capabilities = _capabilities;
    h->nodeCount = _nodes;
    h->flagCount = _flagCount;

    h->forwardOffset = 0;
    h->backwardOffset = _capabilities.has( Capability::ForwardEdges )
        ? edgeData : 0;
    h->nodesOffset = h->backwardOffset
        + (_capabilities.has( Capability::BackwardEdges ) ? edgeData : 0);
    h->flagMapOffset = h->nodesOffset
        + (_capabilities.has( Capability::Nodes ) ? nodeData : 0);
    h->flagsOffset = h->flagMapOffset + flagNames;
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
