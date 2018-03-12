/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cassert>
#include <limits>
#include <array>

struct Node {
    Node( int val ) : val( val ) {}

    int val;
    Node * next = nullptr;
};

template< typename Array >
struct List {
    List( Array & ns ) {
        for ( auto& n : ns ) {
            n.next = head;
            head = &n;
        }
    }

    Node * head = nullptr;

    Node * at( int i ) {
        return at_impl( head, i );
    }

    Node * at_impl( Node * node, int i ) {
        if ( i == 0 )
            return node;
        if ( node == nullptr )
            return nullptr;
        return at_impl( node->next, i - 1 );
    }
};

int main() {
    auto ns = std::array< Node, 3 >( { 1, 2, 3 } );
    auto list = List< decltype( ns ) >( ns );

    _SYM unsigned int i;
    auto res = list.at( i );
    if ( i > 2 )
        assert( res == nullptr );
    else
        assert( res->val == 1 || res->val == 2 || res->val == 3 );
    return 0;
}
