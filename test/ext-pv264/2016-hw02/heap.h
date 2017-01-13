#include <functional> // less
#include <initializer_list>
#include <vector>
#include <memory>
#include <algorithm>
#include <iterator>

#ifndef CPP14_HEAP
#define CPP14_HEAP

/*
The heap type, parametrized by the type of elements and by the type that
defines a comparator. The comparator is expected to be stateless and default
constructible, so it does not need to be stored in the Heap. The ordering of
the heap depends on the type supplied to Compare, for example, with std::less<
T > it creates a max-heap (this behaviour mirrors the one of
std::priority_queue), see the end of this file for some pre-defined heaps with
comparator.

Unlike std::priority_queue, this Heap type supports in-place update of a value.
For this reason it defines the Handle member type which represents a handle to
an element of the heap. This handle needs to remain valid until the element is
removed from the heap, or the heap is destructed. The handle can be used to
update or remove the value from the heap, and to read the value. However, Heap
does not define iterators.

You should implement Heap as standard binary heap in an array
<https://en.wikipedia.org/wiki/Binary_heap>, using std::vector as the
underlying container (for the purpose of complexity analysis, you can consider
push_back to have constant complexity, although in fact it is only amortized
constant). For each of the operations there is a complexity requirement that you
have to meet. You are free to choose the type of the elements stored in the vector
and you can use any additional data structures that you wish.
 */
template< typename T, typename Compare >
struct Heap {

    using value_type = T;

    /*
    Handle has to be default constructible, copy and move constructible, copy
    and move assignable, and support equality. Otherwise it is an opaque data
    structure (it does not define any interface).
     */
    struct Handle {
        Handle() : index( nullptr ) { }

        bool operator==( const Handle &o ) const {
            return index == o.index;
        }

        bool operator!=( const Handle &o ) const {
            return !(*this == o);
        }
        /* ... */
      private:
        friend struct Heap;
        Handle( size_t *ix ) : index( ix ) { }
        size_t *index;
    };

    // O(1). Heap is default constructible in constant time.
    Heap() = default;

    // O(n) where n is the size of other. Heap is copy constructible. Copy does
    // not affect other (and its handles) in any way, the handles from other
    // should not be used with this.
    Heap( const Heap & other ) = default;

    // O(1). Heap is move constructible. After the move, no operations other
    // than destruction or assignment should be done with other and all handles
    // for other should now be valid handles for this.
    Heap( Heap && other ) = default;

    // O(n) where n is the distance from begin to end. Heap can be created from
    // an iterator range in linear time (provided that the iterator has
    // constant time dereference and increment).
    template< typename Iterator >
    Heap( Iterator begin, Iterator end ) {
        std::transform( begin, end, std::back_inserter( _vec ),
            [i = 0]( const T &val ) mutable { return Storage( val, i++ ); } );
        for ( int i = size(); i >= 0; --i )
            _downHeapify( i );
    }

    // O(n) where n is the number of elements in list.
    Heap( std::initializer_list< T > list ) : Heap( list.begin(), list.end() ) { }

    // O(n) where n is the size of other. Heap is copy assignable. Assignment
    // does not affect other in any way, the handles from other should not be
    // used with this.
    Heap &operator=( const Heap & other ) = default;

    // O(1). Heap is move assignable. After the move-assign, no operations
    // other than destruction or assignment should be done with other and all
    // handles for other should now be valid handles for this.
    Heap &operator=( Heap && other ) = default;

    // O(n). Invalidates all handles to this.
    ~Heap() = default;

    // O(1). Heap is swappable. After the swap all handles for this should be
    // valid handles for other and vice versa.
    void swap( Heap &other ) {
        using std::swap;
        swap( _vec, other._vec );
    }

    // O(1). Get the top (e.g. maximal for MaxHeap) element of the heap.
    const T &top() const { return _vec.front()._value; }

    // O(1). Get handle to the top element of the heap.
    Handle topHandle() const { return _vec.front().handle(); }

    // O(log n). Remove the top element from the heap. This invalidates handle
    // to the removed element, handles to other elements must remain valid.
    void pop() {
        _vec.front().swap( _vec.back() );
        _vec.pop_back();
        _downHeapify( 0 );
    }

    // O(log n). Insert an element and return a handle for it. No handles are
    // invalidated.
    Handle insert( const T & value ) {
        size_t ix = _vec.size();
        _vec.emplace_back( value, ix );
        return _upHeapify( ix );
    }

    // O(log n). A version of insert which moves the element into the
    // underlying container instead of copying it.
    Handle insert( T && value ) {
        size_t ix = _vec.size();
        _vec.emplace_back( std::move( value ), ix );
        return _upHeapify( ix );
    }

    // O(1). Get value of an element represented by the given handle.
    // Precondition: h must be a valid handle for this.
    const T &get( const Handle &h ) const {
        return _vec[ *h.index ]._value;
    }

    // O(log n). Update the value represented by the given handle (replace it
    // by the new value).
    // Precondition: h must be a valid handle for this.
    void update( const Handle &h, const T &value ) {
        _vec[ *h.index ]._value = value;
        _update( *h.index );
    }

    // O(log n). A version of update which uses move assign instead of copy
    // assign to replace the value.
    void update( const Handle &h, T &&value ) {
        _vec[ *h.index ]._value = std::move( value );
        _update( *h.index );
    }

    // O(log n). Erase the value represented by the given handle from the heap.
    // Invalidates h, but does not invalidate handles to other elements.
    void erase( const Handle &h ) {
        auto ix = *h.index;
        _vec[ ix ].swap( _vec.back() );
        _vec.pop_back();
        _update( ix );
    }

    // O(1). Get size (number of elements) of the heap.
    size_t size() const { return _vec.size(); }

    // O(1). Is the heap empty?
    bool empty() const { return _vec.empty(); }

    /* ... */
  private:
    Handle _upHeapify( size_t ix ) {
        while ( ix > 0 && _vec[ _up( ix ) ] < _vec[ ix ] ) {
            _vec[ _up( ix ) ].swap( _vec[ ix ] );
            ix = _up( ix );
        }
        return _vec[ ix ].handle();
    }

    void _downHeapify( size_t ix ) {
        while ( _left( ix ) < size() ) { // at least one successor
            size_t maxsuccid = _left( ix );
            auto *maxsucc = &_vec[ maxsuccid ];
            if ( _right( ix ) < size() && *maxsucc < _vec[ _right( ix ) ] ) {
                maxsuccid = _right( ix );
                maxsucc = &_vec[ _right( ix ) ];
            }
            if ( _vec[ ix ] < *maxsucc ) {
                _vec[ ix ].swap( *maxsucc );
                ix = maxsuccid;
            } else
                break;
        }
    }

    void _update( size_t ix ) {
        if ( ix > 0 && _vec[ _up( ix ) ] < _vec[ ix ] )
            _upHeapify( ix );
        else
            _downHeapify( ix );
    }

    size_t _left( size_t ix ) { return 2 * ix + 1; }
    size_t _right( size_t ix ) { return _left( ix ) + 1; }
    size_t _up( size_t ix ) { return (ix - 1) / 2; }

    struct Storage {
//        Storage() : _value(), _handle( nullptr ) { }
        Storage( T &&val, size_t h ) :
            _value( std::move( val ) ), _handle( new size_t( h ) )
        { }

        Storage( const T &val, size_t h ) :
            _value( val ), _handle( new size_t( h ) )
        { }

        Storage( const Storage &o ) :
            _value( o._value ), _handle( o._handle ? new size_t( *o._handle ) : nullptr )
        { }
        Storage( Storage && ) = default;

        Storage &operator=( const Storage &o ) {
            _value = o._value;
            _handle.reset( o._handle ? new size_t( *o._handle ) : nullptr );
            return *this;
        }
        Storage &operator=( Storage &&o ) = default;

        friend bool operator<( const Storage &a, const Storage &b ) {
            return Compare()( a._value, b._value );
        }

        friend bool operator>=( const Storage &a, const Storage &b ) {
            return !(a < b);
        }

        void swap( Storage &other ) {
            using std::swap;
            swap( _value, other._value );
            // swap _handle pointers so that _handles point to the new location
            swap( _handle, other._handle );
            // but also update the pointed-to indices
            swap( *_handle, *other._handle );
        }

        Handle handle() const { return { _handle.get() }; }

        T _value;
        std::unique_ptr< size_t > _handle;
    };

    std::vector< Storage > _vec;
};

// O(n log n). Assigns values of the heap in the sorted order (top first) to the output
// iterator. The complexity should hold if both increment and assignment to o
// can be done in constant time.
template< typename OutputIterator, typename T, typename Cmp >
void copySorted( Heap< T, Cmp > heap, OutputIterator o ) {
    for ( ; !heap.empty(); heap.pop(), ++o )
        *o = heap.top();
}

// O(n log n). Create sorted vector form the given heap.
template< typename T, typename Cmp >
std::vector< T > toSortedVector( Heap< T, Cmp > heap ) {
    std::vector< T > vec;
    vec.reserve( heap.size() );
    copySorted( std::move( heap ), std::back_inserter( vec ) );
    return vec;
}

// O(1). Swaps two heaps. See Heap::swap for more.
template< typename T, typename Cmp >
void swap( Heap< T, Cmp > & a, Heap< T, Cmp > & b ) { a.swap( b ); }

// examples of concrete heaps

template< typename T >
using MaxHeap = Heap< T, std::less< T > >;

template< typename T >
using MinHeap = Heap< T, std::greater< T > >;

template< typename A, typename B, typename Cmp >
struct PairCompare {
    bool operator()( const std::pair< A, B > & a, const std::pair< A, B > & b ) const {
        return Cmp()( a.first, b.first );
    }
};

template< typename P, typename V, typename Cmp = std::less< P > >
using PriorityQueue = Heap< std::pair< P, V >, PairCompare< P, V, Cmp > >;


#endif // CPP14_HEAP

