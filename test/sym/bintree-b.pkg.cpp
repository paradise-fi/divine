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
    using value_type = int32_t;

    struct Node {
        Node( value_type v = 0 ) : val( v ) {}

        Node * min() { return ( left == nullptr ) ? this : left->min(); }
        Node * succ() { return ( right == nullptr ) ? nullptr : right->min(); }

        value_type val;
        Node *left = nullptr, *right = nullptr, *parent = nullptr;
    };

    void insert( Node *node ) {
        if ( !root )
            root = node;
        else
            insert_impl( root, node );
    }

    void insert_impl( Node *node, Node *in ) {
        if ( in->val < node->val )
            if ( !node->left ) {
                node->left = in;
                in->parent = node;
            } else {
                insert_impl( node->left, in );
            }
        else
            if ( !node->right ) {
                node->right = in;
                in->parent = node;
            } else {
                insert_impl( node->right, in );
            }
    }

    void transplant(Node * u, Node * v) {
        if( u->parent == nullptr )
            root = v;
        else if( u == u->parent->left )
            u->parent->left = v;
        else
            u->parent->right = v;

        if( v != nullptr )
            v->parent = u->parent;
    }

    void erase( value_type v ) {
        if ( Node * n = search( v ) )
            erase_impl( n );
    }

    void erase_impl( Node * node ) {
        if ( node->left == nullptr ) {
            transplant( node, node->right );
        } else if ( node->right == nullptr ) {
            transplant( node, node->left );
        } else {
            Node* succ = node->succ();
            if ( succ->parent != node ) {
                transplant( succ, succ->right );
                succ->right = node->right;
                node->right->parent = succ;
            }
            transplant( node, succ );
            succ->left = node->left;
            node->left->parent = succ;
        }
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
    int x = static_cast< int >( __lamp_any_i32() );
    return x;
}

int main() {
    BinTree bt;

    auto n1 = make_abstract_node();
    auto n2 = make_abstract_node();

    bt.insert( &n1 );
    bt.insert( &n2 );

    bt.erase( n2.val );

    assert( bt.contains( n1.val ) );
    if ( n1.val != n2.val )
        assert( !bt.contains( n2.val ) );
}
