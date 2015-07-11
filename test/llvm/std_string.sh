. lib
. flavour vanilla

llvm_verify_cpp valid <<EOF
#include <string>

int main() {
    std::string s( "hello" );
    return 0;
}
EOF

llvm_verify_cpp valid <<EOF
#include <string>

int main() {
    std::string s("/foo/bar");
    auto n = std::find_if( s.begin(), s.end(), []( char c ) { return c == '/'; } );
    return 0;
}
EOF

llvm_verify_cpp valid <<EOF
#include <string>

int main() {
    std::string s("/foo/bar"), t( s.begin(), s.end() );
    return 0;
}
EOF

exit 0

llvm_verify_cpp valid <<EOF
#include <string>

int main() {
    std::string s("/foo/bar");
    auto n = std::find_if( s.begin(), s.end(), []( char c ) { return c == '/'; } );
    std::string ch1( s.begin(), n );
    return 0;
}
EOF

llvm_verify_cpp valid <<EOF
#include <fs-path.h>

int main() {
    auto sp = divine::fs::path::splitPath( "/foo/bar" );
    return 0;
}
EOF

