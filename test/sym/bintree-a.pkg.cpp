/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
// V: v.O1 CC_OPT: -O1
// V: v.O2 CC_OPT: -O2
// V: v.Os CC_OPT: -Os
#include <sys/lamp.h>
#include <cstdint>
#include <limits>
#include <array>
#include <cassert>

struct BinTree {
    using value_type = uint64_t;

    struct Node {
        Node( value_type v = 0 ) : val( v ){}

        value_type val;
        Node *left = nullptr, *right = nullptr;
    };

    void insert( Node *node ) {
        if ( !root )
            root = node;
        else
            insert_impl( root, node );
    }

    void insert_impl( Node *node, Node *in ) {
        if ( in->val < node->val )
            if ( !node->left )
                node->left = in;
            else
                insert_impl( node->left, in );
        else
            if ( !node->right )
                node->right = in;
            else
                insert_impl( node->right, in );
    }

    Node * search( value_type v ) const {
        return search_impl( root, v );
    }

    Node * search_impl( Node * node, value_type v ) const {
        if ( !node )
            return nullptr;
        else if ( node->val == v )
            return node;
        else if ( node->val < v )
            return search_impl( node->right, v );
        else
            return search_impl( node->left, v );
    }

    bool contains( value_type v ) {
        return search( v ) != nullptr;
    }

    bool is_correct() {
        return is_correct_impl( root,
            std::numeric_limits< value_type >::min(),
            std::numeric_limits< value_type >::max() );
    }

    bool is_correct_impl( Node *node, value_type min, value_type max ) {
        if ( !node ) return true;
        if ( node->val < min || node->val > max )
            return false;
        return is_correct_impl( node->left, min, node->val ) &&
               is_correct_impl( node->right, node->val, max );
    }


    Node * root = nullptr;
};

BinTree::Node make_abstract_node() {
    uint64_t x = __lamp_any_i64();
    return x;
}

int main() {
    BinTree bt;

    auto n1 = make_abstract_node();
    auto n2 = make_abstract_node();
    auto n3 = make_abstract_node();

    bt.insert( &n1 );
    bt.insert( &n2 );
    bt.insert( &n3 );

    assert( bt.is_correct() );
    assert( bt.contains( n1.val ) );
    assert( bt.contains( n2.val ) );
    assert( bt.contains( n3.val ) );
}
