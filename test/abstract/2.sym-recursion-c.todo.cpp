/* VERIFY_OPTS: --symbolic */

#include <cassert>
#include <limits>
#include <initializer_list>
#define __sym __attribute__((__annotate__("lart.abstract.symbolic")))

struct Node {
    Node( int val, Node * next ) : val( val ), next( next ) {}

    int val;
    Node * next;
};

struct List {
    List( std::initializer_list< int > list ) {
        for ( const auto val : list ) {
            auto node = new Node( val, head );
            head = node;
        }
    }

    Node * head;

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
    __sym int i;
    List list = { 1, 2, 3 };
    assert( list.at( i ) == nullptr || list.at( i )->val <= 3 );

    return 0;
}
