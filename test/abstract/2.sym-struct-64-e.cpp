/* VERIFY_OPTS: --symbolic */
#include <abstract/domains.h>
#include <cstdint>
#include <limits>
#include <array>
#include <cassert>

struct BinTree {
    using value_type = int64_t;

    struct Node {
        Node( value_type v = 0 ) : val( v ){}

        value_type val;
        Node *left = nullptr, *right = nullptr;
    };

    void insert( Node *node ) {
        root = insert_impl( root, node );
    }

    Node * insert_impl( Node *node, Node *in ) {
        assert( in );
        if ( !node )
            return in;
        if ( in->val < node->val )
            node->left = insert_impl( node->left, in );
        else
            node->right = insert_impl( node->right, in );
        return node;
    }

    bool is_correct_impl( Node *node, value_type min, value_type max ) {
        if ( !node ) return true;
        if ( node->val < min || node->val > max )
            return false;
        return is_correct_impl( node->left, min, node->val - 1 ) &&
               is_correct_impl( node->right, node->val, max );
    }

    bool is_correct() {
        return is_correct_impl( root,
            std::numeric_limits< value_type >::min(),
            std::numeric_limits< value_type >::max() );
    }

    Node * root = nullptr;
};

BinTree::Node make_abstract_node() {
    _SYM int x;
    return { x };
}

int main() {
    BinTree bt;

    std::array< BinTree::Node, 100 > ns;
    std::generate( ns.begin(), ns.end(), [] { return make_abstract_node(); } );

    for ( auto & n : ns )
        bt.insert( &n );

    assert( bt.is_correct() );
}
