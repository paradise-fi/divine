#include <memory>

#ifndef _FS_INODE_H_
#define _FS_INODE_H_

namespace divine {
namespace fs {

struct Mode {
    static const unsigned TMASK    = 0170000;
    static const unsigned SOCKET   = 0140000;
    static const unsigned LINK     = 0120000;
    static const unsigned FILE     = 0100000;
    static const unsigned BLOCKD   = 0060000;
    static const unsigned DIR      = 0040000;
    static const unsigned CHARD    = 0020000;
    static const unsigned FIFO     = 0010000;

    static const unsigned SUID     = 0004000;
    static const unsigned GUID     = 0002000;
    static const unsigned STICKY   = 0001000;

    static const unsigned RWXUSER  = 0000700;
    static const unsigned RUSER    = 0000400;
    static const unsigned WUSER    = 0000200;
    static const unsigned XUSER    = 0000100;

    static const unsigned RWXGROUP = 0000070;
    static const unsigned RGROUP   = 0000040;
    static const unsigned WGROUP   = 0000020;
    static const unsigned XGROUP   = 0000010;

    static const unsigned RWXOTHER = 0000007;
    static const unsigned ROTHER   = 0000004;
    static const unsigned WOTHER   = 0000002;
    static const unsigned XOTHER   = 0000001;

    Mode( unsigned m ) : _mode( m ) {}
    Mode( const Mode & ) = default;
    Mode &operator=( const Mode & ) = default;
    operator unsigned() const {
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

    unsigned _mode;
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

    INode( unsigned mode, Ptr data = nullptr ) :
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
