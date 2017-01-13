#include <cassert>

int main() {
    try {
        throw 4;
    } catch ( ... ) {
        return 0;
    }
    assert( false );
    return 1;
}
