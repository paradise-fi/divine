#include <memory>

#include <sys/types.h>

#ifndef _FS_INODE_H_
#define _FS_INODE_H_

namespace divine {
namespace fs {

struct Mode {
    static const mode_t TMASK    = 0170000;
    static const mode_t SOCKET   = 0140000;
    static const mode_t LINK     = 0120000;
    static const mode_t FILE     = 0100000;
    static const mode_t BLOCKD   = 0060000;
    static const mode_t DIR      = 0040000;
    static const mode_t CHARD    = 0020000;
    static const mode_t FIFO     = 0010000;

    static const mode_t SUID     = 0004000;
    static const mode_t GUID     = 0002000;
    static const mode_t STICKY   = 0001000;

    static const mode_t RWXUSER  = 0000700;
    static const mode_t RUSER    = 0000400;
    static const mode_t WUSER    = 0000200;
    static const mode_t XUSER    = 0000100;

    static const mode_t RWXGROUP = 0000070;
    static const mode_t RGROUP   = 0000040;
    static const mode_t WGROUP   = 0000020;
    static const mode_t XGROUP   = 0000010;

    static const mode_t RWXOTHER = 0000007;
    static const mode_t ROTHER   = 0000004;
    static const mode_t WOTHER   = 0000002;
    static const mode_t XOTHER   = 0000001;

    // composites
    static const mode_t GRANTS   = 0000777;
    static const mode_t CHMOD    = 0007777;

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

struct DataItem {
    virtual ~DataItem() {}

    virtual size_t size() const = 0;

    template< typename T >
    T *as() {
        return dynamic_cast< T * >( this );
    }

    template< typename T >
    const T *as() const {
        return dynamic_cast< const T * >( this );
    }
};

using Handle = std::unique_ptr< DataItem >;
using Ptr = DataItem *;
using ConstPtr = const DataItem *;

struct INode {

    INode( mode_t mode, Ptr data = nullptr ) :
        _mode( mode ),
        _ino( getIno() ),
        _uid( 0 ),
        _gid( 0 ),
        _data( data )
    {}

    bool assign( Ptr item ) {
        if ( _data )
            return false;
        _data.reset( item );
        return true;
    }

    unsigned ino() const {
        return _ino;
    }

    size_t size() const {
        return _data ? _data->size() : 0;
    }

    Mode mode() const {
        return _mode;
    }
    Mode &mode() {
        return _mode;
    }

    unsigned uid() const {
        return _uid;
    }
    unsigned gid() const {
        return _gid;
    }

    Ptr data() {
        return _data.get();
    }
    ConstPtr data() const {
        return _data.get();
    }

    explicit operator bool() const {
        return bool( _data );
    }

private:
    Mode _mode;
    unsigned _ino;
    unsigned _uid;
    unsigned _gid;
    Handle _data;

    static unsigned getIno() {
        static unsigned ino = 0;
        return ++ino;
    }
};

using Node = std::shared_ptr< INode >;
using WeakNode = std::weak_ptr< INode >;

} // namespace fs
} // namespace divine

#endif
