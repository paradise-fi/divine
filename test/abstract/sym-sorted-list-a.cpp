/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cstdint>
#include <cassert>

struct Node {
    int64_t val;
    Node * next = nullptr;

};

struct List {
    Node * head = nullptr;

    void insert( Node * node ) {
        if ( !head ) {
            head = node;
        } else if ( node->val < head->val ) {
            node->next = head;
            head = node;
        } else {
            auto curr = head;
            while ( curr->next && curr->next->val < node->val )
                curr = curr->next;
            node->next = curr->next;
            curr->next = node;
        }
    }

    bool isSorted() {
        Node * curr = head;
        while ( curr ) {
            auto next = curr->next;
            if ( next && curr->val > next->val )
                return false;
            curr = curr->next;
        }
        return true;
    }
};

void init_node( Node * node ) {
    _SYM int x;
    node->val = x;
}

int main() {
    Node n1, n2, n3;
    init_node( &n1 );
    init_node( &n2 );
    init_node( &n3 );

    List list;
    list.insert( &n1 );
    list.insert( &n2 );
    list.insert( &n3 );

    assert( list.isSorted() );
}


