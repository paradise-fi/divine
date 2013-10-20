#if __cplusplus >= 201103L
#include <wibble/raii.h>
using namespace wibble::raii;
#endif

#include <wibble/test.h>


struct TestRAII {
#if __cplusplus >= 201103L
    Test basic() {
        int y = 0;
        {
            auto del = autoDeleter( []() -> int { return 5; }, [ &y ]( int x ) {
                    assert_eq( x, 10 );
                    y = x;
                } );
            assert_eq( y, 0 );
            assert_eq( del.value, 5 );
            del.value = 10;
        }
        assert_eq( y, 10 );
    }

    Test ref() {
        int x = 5;
        {
            auto del = refDeleter( x, []( int &y ) {
                    y = 10;
                } );
            assert_eq( del.value, 5 );
            assert_eq( x, 5 );
        }
        assert_eq( x, 10 );
    }

    Test refIf() {
        int x = 5;
        {
            auto del = refDeleteIf( true, x, []( int &y ) { y = 10; } );
            assert_eq( x, 5 );
        }
        assert_eq( x, 10 );
        {
            auto del = refDeleteIf( false, x, []( int &y ) { y = 15; } );
            assert_eq( x, 10 );
        }
        assert_eq( x, 10 );
    }

    static void delFn( int &x ) { x = 0; }

    Test fn() {
        int x = 5;
        {
            AutoDelete< int & > del( x, delFn );
            assert_eq( x, 5 );
        }
        assert_eq( x, 0 );
    }
#else /* FIXME: workaround */
    void basic() {}
    void ref() {}
    void refIf() {}
    void fn() {}
#endif
};
