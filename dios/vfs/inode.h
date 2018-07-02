// -*- C++ -*- (c) 2015 Jiří Weiser

#include <memory>

#include <sys/types.h>

#ifndef _FS_INODE_H_
#define _FS_INODE_H_

namespace __dios {
namespace fs {

struct Mode {
    enum : mode_t {
       TMASK    = 0170000,
       SOCKET   = 0140000,
       LINK     = 0120000,
       FILE     = 0100000,
       BLOCKD   = 0060000,
       DIR      = 0040000,
       CHARD    = 0020000,
       FIFO     = 0010000,

       SUID     = 0004000,
       GUID     = 0002000,
       STICKY   = 0001000,

       RWXUSER  = 0000700,
       RUSER    = 0000400,
       WUSER    = 0000200,
       XUSER    = 0000100,

       RWXGROUP = 0000070,
       RGROUP   = 0000040,
       WGROUP   = 0000020,
       XGROUP   = 0000010,

       RWXOTHER = 0000007,
       ROTHER   = 0000004,
       WOTHER   = 0000002,
       XOTHER   = 0000001,

    // composites
       GRANTS   = 0000777,
       CHMOD    = 0007777,
    };

    Mode( mode_t m ) : _mode( m ) {}
    Mode( const Mode & ) = default;
    Mode &operator=( const Mode & ) = default;
    operator mode_t() const {
        return _mode;
    }

    bool isSocket() const {
        return _is( SOCKET );
    }
    bool isLink() const {
        return _is( LINK );
    }
    bool isFile() const {
        return _is( FILE );
    }
    bool isBlockDevice() const {
        return _is( BLOCKD );
    }
    bool isDirectory() const {
        return _is( DIR );
    }
    bool isCharacterDevice() const {
        return _is( CHARD );
    }
    bool isFifo() const {
        return _is( FIFO );
    }

    bool hasSUID() const {
        return _has( SUID );
    }
    bool hasGUID() const {
        return _has( GUID );
    }
    bool hasStickyBit() const {
        return _has( STICKY );
    }

    bool userRead() const {
        return _has( RUSER );
    }
    bool userWrite() const {
        return _has( WUSER );
    }
    bool userExecute() const {
        return _has( XUSER );
    }

    bool groupRead() const {
        return _has( RGROUP );
    }
    bool groupWrite() const {
        return _has( WGROUP );
    }
    bool groupExecute() const {
        return _has( XGROUP );
    }

    bool otherRead() const {
        return _has( ROTHER );
    }
    bool otherWrite() const {
        return _has( WOTHER );
    }
    bool otherExecute() const {
        return _has( XOTHER );
    }

private:
    bool _is( unsigned p ) const {
        return _check( p, TMASK );
    }

    bool _has( unsigned p ) const {
        return _check( p, p );
    }

    bool _check( unsigned p, unsigned mask ) const {
        return ( _mode & mask ) == p;
    }

    mode_t _mode;
};

struct FileDescriptor;

struct INode
{
    INode() :
        _mode( 0 ),
        _ino( getIno() ),
        _uid( 0 ),
        _gid( 0 )
    {}

    virtual ~INode() {}

    template< typename T >
    T *as() {
        return dynamic_cast< T * >( this );
    }

    template< typename T >
    const T *as() const {
        return dynamic_cast< const T * >( this );
    }

    virtual size_t size() const { return 0; }
    virtual bool read( char *, size_t, size_t & ) { return false; }
    virtual bool write( const char *, size_t, size_t & ) { return false; }
    virtual bool canRead() const { return false; }
    virtual bool canWrite() const { return false; }
    virtual void clear() {}

    virtual void open( FileDescriptor & ) {}
    virtual void close( FileDescriptor & ) {}

    Mode mode() const { return _mode; }
    void mode( mode_t mode ) { _mode = mode; }

    unsigned ino() const { return _ino; }
    unsigned uid() const { return _uid; }
    unsigned gid() const { return _gid; }

private:
    Mode _mode;
    unsigned _ino;
    unsigned _uid;
    unsigned _gid;

    static unsigned getIno()
    {
        static unsigned ino = 0;
        return ++ino;
    }
};

using Node = std::shared_ptr< INode >;
using WeakNode = std::weak_ptr< INode >;

} // namespace fs
} // namespace __dios

#endif
