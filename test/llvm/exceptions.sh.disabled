. lib
. flavour vanilla

llvm_verify_cpp invalid ASSERTION testcase.cpp:5 <<EOF
#include <cassert>
int main() {
    try { throw 5; }
    catch( ... ) {}
    assert( 0 );
    return 0;
}
EOF

llvm_verify_cpp valid <<EOF
#include <cassert>
#include <exception>

int a = 0;
int b = 0;

struct destructible {
    ~destructible() { a = 1; }
};

int throws() {
    std::exception blergh;
    throw blergh;
}

int dt() {
    destructible x;
    try {
        throws();
    } catch (...) {
        a = b = 3;
        throw;
    }
}

int main() {
    int x = 0;
    try {
        x = 3;
        dt();
    } catch (std::exception &e) {
        x = 8;
    } catch (...) {
        x = 2;
    }
    assert( x == 8 );
    assert( a == 1 );
    assert( b == 3 );
    return 0;
}
EOF

llvm_verify_cpp valid <<EOF
#include <cassert>
int main() {
    try { throw 5; }
    catch( int x ) {
        assert( x == 5 );
        return 0;
    }
    assert( 0 );
    return 0;
}
EOF

llvm_verify_cpp valid <<EOF
#include <cassert>
int main() {
    try { throw 5; }
    catch( int &x ) {
        assert( x == 5 );
        return 0;
    }
    assert( 0 );
    return 0;
}
EOF

llvm_verify_cpp valid <<EOF
#include <cassert>
#include <stdexcept>
int main() {
    int x = 0;
    try { throw std::logic_error( "moo" ); }
    catch( std::exception ) {
        x = 5;
    }
    assert( x == 5 );
    return 0;
}
EOF

