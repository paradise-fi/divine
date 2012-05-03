// -*- C++ -*- (c) 2012 Petr Rockai <me@mornfall.net>

#include <divine/bitstream.h>
#include <wibble/test.h>

using namespace divine;

struct TestBitstream {
    Test _bitstream() {
        bitstream bs;
        bs << 1 << 2 << 3;
        int x;
        bs >> x; assert_eq( x, 1 );
        bs >> x; assert_eq( x, 2 );
        bs >> x; assert_eq( x, 3 );
        assert( bs.empty() );
    }

    Test _bitblock() {
        bitblock bs;
        bs << 1 << 2 << 3;
        int x;
        bs >> x; assert_eq( x, 1 );
        bs >> x; assert_eq( x, 2 );
        bs >> x; assert_eq( x, 3 );
        assert( bs.empty() );
    }
};
