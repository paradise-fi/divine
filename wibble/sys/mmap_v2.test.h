// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>
#if __cplusplus >= 201103L
#include <wibble/sys/mmap_v2.h>
using namespace wibble::sys;
#endif

#include <wibble/test.h>
#include <string.h>

using namespace std;
using namespace wibble;

struct TestMMapV2 {
    Test read() {
#if defined POSIX && __cplusplus >= 201103L
        MMap map;
        assert_eq( map.size(), 0U );
        assert( !map );
        assert( !map.valid() );
        assert( !map.mode() );

        map.map( "/bin/sh" );
        assert_neq( map.size(), 0U );
        assert_eq( map.mode(), MMap::ProtectMode::Read | MMap::ProtectMode::Shared );
        assert( map.valid() );
        assert_eq( map[ 1 ], 'E' );
        assert_eq( map[ 2 ], 'L' );
        assert_eq( map[ 3 ], 'F' );

        MMap map1 = map; // shared_ptr semantics
        assert_eq( map.size(), map.size() );
        assert_eq( map.asArrayOf< char >(), map1.asArrayOf< char >() );
        assert_eq( map.mode(), map1.mode() );

        assert_eq( map1.get< char >( 1 ), 'E' );
        assert_eq( map1.get< char >( 2 ), 'L' );
        assert_eq( map1.get< char >( 3 ), 'F' );

        map1.unmap();
        assert_eq( map1.size(), 0U );
        assert( !map1 );
        assert_eq( map.cget< char >( 1 ), 'E' );
        assert_eq( map.cget< char >( 2 ), 'L' );
        assert_eq( map.cget< char >( 3 ), 'F' );

        assert( map.valid() );

        map.unmap();
        assert_eq( map.size(), 0U );
        assert( !map );
#endif
    }
};
