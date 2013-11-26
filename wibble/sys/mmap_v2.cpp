#if __cplusplus >= 201103L
#ifdef POSIX

#include <wibble/sys/mmap_v2.h>
#include <wibble/exception.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

using namespace wibble::sys;
using wibble::operator|;

int mmapProt( MMap::ProtectModeFlags flags ) {
    int prot = 0;
    if ( flags.has( MMap::ProtectMode::Read ) ) prot |= PROT_READ;
    if ( flags.has( MMap::ProtectMode::Write ) ) prot |= PROT_WRITE;
    if ( flags.has( MMap::ProtectMode::Execute ) ) prot |= PROT_EXEC;
    return prot;
}

int mmapFlags( MMap::ProtectModeFlags flags ) {
    int mf = 0;
    if ( flags.has( MMap::ProtectMode::Shared ) ) mf |= MAP_SHARED;
    if ( flags.has( MMap::ProtectMode::Private ) ) mf |= MAP_PRIVATE;
    return mf;
}

int openFlags( MMap::ProtectModeFlags flags ) {
    if ( flags.has( MMap::ProtectMode::Read ) &&
            flags.has( MMap::ProtectMode::Write ) )
        return O_RDWR;
    if ( flags.has( MMap::ProtectMode::Read ) )
        return O_RDONLY;
    if ( flags.has( MMap::ProtectMode::Write ) )
        return O_WRONLY;
    assert_unreachable( "No open flags specified" );
}

MMap::MMap( const std::string &file, ProtectModeFlags flags ) :
    _flags( flags ), _size( 0 )
{
    _map( file );
}

MMap::MMap( int fd, ProtectModeFlags flags ) : _flags( flags ), _size( 0 ) {
    _map( fd );
}

void MMap::map( const std::string &file, ProtectModeFlags flags ) {
    _flags = flags;
    _map( file );
}

void MMap::map( int fd, ProtectModeFlags flags ) {
    _flags = flags;
    _map( fd );
}

void MMap::unmap() {
    _ptr = nullptr;
    _size = 0;
}

void MMap::_map( const std::string &file ) {
    int fd = ::open( file.c_str(), openFlags( _flags ) );
    if ( fd < 0 )
        throw wibble::exception::System( "opening file failed: " + file );
    _map( fd );
}

void MMap::_map( int fd ) {
    struct stat st;
    if ( fstat( fd, &st ) != 0 )
        throw wibble::exception::System( "stat failed while mmaping" );
    size_t size = _size = st.st_size;

    void *ptr = ::mmap( nullptr, _size, mmapProt( _flags ), mmapFlags( _flags ), fd, 0 );
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
    if ( ptr == MAP_FAILED )
        throw wibble::exception::System( "mmaping file failed" );
#pragma GCC diagnostic pop

    _ptr = std::shared_ptr< void >( ptr,
        [ fd, size ]( void *h ) {
            ::munmap( h, size );
            ::close( fd );
        } );
}



#endif // POSIX
#endif // __cplusplus
