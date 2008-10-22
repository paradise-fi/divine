// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <divine/blob.h>

using namespace divine;

struct TestBlob {
    Test basic() {
        Blob b( 32 );
        b.get< int >() = 42;
        assert_eq( b.get< int >(), 42 );
    }
};
