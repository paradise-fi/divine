#pragma once

#include <rst/common.h>
#include <util/array.hpp>
#include <rst/sym.h>
#include <assert.h>

namespace abstract::mstring {

    namespace sym {
    namespace {
        struct Value
        {
            Value() = default;
            Value( const Value & other ) = default;
            Value( Value && other ) = default;

            Value& operator=(const Value& other) = default;
            Value& operator=(Value&& other) = default;

            Value( lart::sym::Formula * ptr ) : ptr( ptr ) {}

            ~Value()
            {
                ::abstract::sym::formula_cleanup_check( ptr );
            }

            //_LART_INLINE
            operator bool() const {
                assert( ptr->type().bitwidth() == 1 );
                auto tr = __sym_bool_to_tristate( ptr );
                if ( __tristate_lower( tr ) ) {
                    __sym_assume( ptr, ptr, true );
                    return true;
                } else {
                    __sym_assume( ptr, ptr, false );
                    return false;
                }
            }

            lart::sym::Formula * ptr;
        };

        //_LART_INLINE
        Value operator+( const Value& l, const Value& r ) noexcept { return __sym_add( l.ptr, r.ptr ); }

        //_LART_INLINE
        Value operator+=( Value& l, const Value& r ) noexcept { return l + r; }

        //_LART_INLINE
        Value operator-( const Value& l, const Value& r ) noexcept { return __sym_sub( l.ptr, r.ptr ); }

        //_LART_INLINE
        Value operator-=( Value& l, const Value& r ) noexcept { return l - r; }

        //_LART_INLINE
        Value operator&&( const Value& l, const Value& r ) noexcept { return __sym_and( l.ptr, r.ptr ); }

        //_LART_INLINE
        Value operator||( const Value& l, const Value& r ) noexcept { return __sym_or( l.ptr, r.ptr ); }

        //_LART_INLINE
        Value operator<( const Value& l, const Value& r ) noexcept { return __sym_icmp_ult( l.ptr, r.ptr ); }

        //_LART_INLINE
        Value operator<=( const Value& l, const Value& r ) noexcept { return __sym_icmp_ule( l.ptr, r.ptr ); }

        //_LART_INLINE
        Value operator>( const Value& l, const Value& r ) noexcept { return __sym_icmp_ugt( l.ptr, r.ptr ); }

        //_LART_INLINE
        Value operator>=( const Value& l, const Value& r ) noexcept { return __sym_icmp_uge( l.ptr, r.ptr ); }

        //_LART_INLINE
        Value operator==( const Value& l, const Value& r ) noexcept { return __sym_icmp_eq( l.ptr, r.ptr ); }

        //_LART_INLINE
        Value operator!=( const Value& l, const Value& r ) noexcept { return __sym_icmp_ne( l.ptr, r.ptr ); }

        //_LART_INLINE
        void assume( Value cond ) noexcept
        {
            __sym_assume( cond.ptr, cond.ptr, true );
        }

        //_LART_INLINE
        Value constant( size_t val ) noexcept
        {
            constexpr auto bw = std::numeric_limits< size_t >::digits;
            return __sym_lift( bw, 1, val );
        }

        //_LART_INLINE
        Value variable() noexcept
        {
            constexpr auto bw = std::numeric_limits< size_t >::digits;
            return __sym_lift( bw, 0 );
        }

        //_LART_INLINE
        auto bounded( const Value& from, const Value& to ) noexcept
        {
            auto var = sym::variable();
            assume( ( from <= var ) && ( var < to ) );
            return var;
        }

        //_LART_INLINE
        template< typename TBound, typename TInterval >
        size_t lower( TBound bound, TInterval interval ) noexcept
        {
            auto val = interval.from + __vm_choose( interval.size() );
            assume( bound == sym::constant( val ) );
            return val;
        }

        template< typename TInterval >
        //_LART_INLINE
        Value make_bounded( const TInterval interval ) noexcept
        {
            return sym::bounded( sym::constant( interval.from ), sym::constant( interval.to ) );
        }

    } // anonymous namespace
    } // namespace sym


    struct Bound
    {
        Bound( size_t from, size_t to )
            : from( from ), to ( to )
        { assert( from < to ); }

        size_t size() const noexcept { return to - from; }

        size_t from, to;

        Bound& operator+=( const Bound& other ) noexcept
        {
            from += other.from;
            to += other.to - 1; // TODO check
            return *this;
        }

        Bound& operator-=( const Bound& other ) noexcept
        {
            from -= other.to + 1;
            to -= other.from;
            return *this;
        }
    };

    //_LART_INLINE
    static Bound operator+( const Bound& l, const Bound& r ) noexcept
    {
        return { l.from + r.from, l.to + r.to - 1};
    }

    //_LART_INLINE
    static Bound operator-( const Bound& l, const Bound& r ) noexcept
    {
        return { l.from - r.to + 1, l.to - r.from };
    }

    struct ConstrainedValue
    {
        using Abstract = sym::Value;
        using Interval = Bound;

        Abstract symbolic;
        Interval interval;

        ConstrainedValue& operator+=( const ConstrainedValue& other ) noexcept
        {
            symbolic += other.symbolic;
            interval += other.interval; // TODO check
            return *this;
        }

        ConstrainedValue& operator-=( const ConstrainedValue& other ) noexcept
        {
            symbolic -= other.symbolic;
            interval -= other.interval; // TODO check
            return *this;
        }
    };

    //_LART_INLINE
    static ConstrainedValue constrained_constant( size_t val ) noexcept
    {
        return { sym::constant( val ), Bound{ val, val + 1 } };
    }

    //_LART_INLINE
    static ConstrainedValue operator+( const ConstrainedValue& l, const ConstrainedValue& r ) noexcept
    {
        return { l.symbolic + r.symbolic, l.interval + r.interval };
    }

    //_LART_INLINE
    static ConstrainedValue operator-( const ConstrainedValue& l, const ConstrainedValue& r ) noexcept
    {
        return { l.symbolic - r.symbolic, l.interval - r.interval };
    }

    template< typename TBound, typename TValue >
    struct Segmentation
    {
        using Values = Array< TValue >;
        using Bounds = Array< TBound >;
        using BoundIt = typename Bounds::iterator;
        using ValueIt = typename Values::iterator;

        using TAbstract = typename TBound::Abstract;
        using TInterval = typename TBound::Interval;

        struct Segment
        {
            BoundIt begin;
            BoundIt end;
            ValueIt value;

            //_LART_INLINE
            TAbstract size() const noexcept { return end->symbolic - begin->symbolic; }

            //_LART_INLINE
            bool empty() const noexcept { return begin->symbolic == end->symbolic; }

            //_LART_INLINE
            bool has_value( const TValue & val ) const noexcept
            {
                if ( !empty() )
                    return *value == val;
                return false;
            }

            Segment& operator++()
            {
                ++begin;
                ++end;
                ++value;
                return *this;
            }
        };

        using Interval = Bound;

        struct View
        {
            TBound offset;
            Range< BoundIt > bounds;
            Range< ValueIt > values;

            //_LART_INLINE
            Segment front() const noexcept
            {
                Segment seg{ bounds.begin(), std::next( bounds.begin() ), values.begin() };
                while ( seg.empty() )
                    ++seg;
                return seg;
            }

            //_LART_INLINE
            TAbstract size() const noexcept
            {
                return bounds.end()->symbolic - offset->symbolic;
            }
        };

        Segmentation()
            : _offset( constrained_constant( 0 ) )
            , _values( std::make_shared< Values >() )
            , _bounds( std::make_shared< Bounds >() )
        {}

        Segmentation( TBound offset
                    , std::shared_ptr< Values > values
                    , std::shared_ptr< Bounds > bounds
        )
            : _offset( offset )
            , _values( values )
            , _bounds( bounds )
        {}

        //_LART_INLINE
        const TBound & begin() const noexcept { return _bounds->front(); }

        //_LART_INLINE
        const TBound & end() const noexcept { return _bounds->back(); }

        //_LART_INLINE
        bool empty() const noexcept { return _bounds->empty(); }

        //_LART_INLINE
        Segment segment( TAbstract idx ) const noexcept
        {
            assert( _bounds->size() >= 2 );

            auto from = _bounds->begin();
            auto to = std::next( _bounds->begin() );
            auto val = _values->begin();
            for (; to != _bounds->end(); ++from, ++to, ++val ) {
                if ( idx >= from->symbolic && idx < to->symbolic ) {
                    return { from, to, val };
                }
            }

            _UNREACHABLE_F( "index out of bounds" );
        }

        //_LART_INLINE
        void append( const TInterval & interval ) noexcept
        {
            auto symbolic = sym::make_bounded( interval );
            if ( !_bounds->empty() ) {
                sym::assume( _bounds->back().symbolic <= symbolic );
            }
            _bounds->push_back( { symbolic, interval } );
        }

        //_LART_INLINE
        void append( char value ) noexcept
        {
            _values->push_back( value );
        }

        //_LART_INLINE
        TValue read() const noexcept { return read( 0 ); }

        //_LART_INLINE
        TValue read( size_t idx ) const noexcept {
            return read( constrained_constant( idx ) );
        }

        //_LART_INLINE
        TValue read( TBound idx ) const noexcept
        {
            idx = idx + _offset;
            assert( idx.symbolic < size() && "access out of bounds" );
            return *segment( idx.symbolic ).value;
        }

        //_LART_INLINE
        void write( char val ) noexcept { write( 0, val ); }

        //_LART_INLINE
        void write( size_t idx, char val ) noexcept
        {
            write( constrained_constant( idx ), val );
        }

        //_LART_INLINE
        void write( TBound idx, TValue val ) noexcept
        {
            idx = idx + _offset;
            assert( idx.symbolic < size() && "access out of bounds" );

            auto seg = segment( idx.symbolic );

            auto one = constrained_constant( 1 );

            if ( seg.has_value( val ) ) {
                // do nothing
            } else if ( seg.size() == sym::constant( 1 ) ) {
                // rewite single character segment
                *seg.value = val;
            } else if ( idx.symbolic == seg.begin->symbolic ) {
                // rewrite first character of a segment
                if ( seg.begin != _bounds->begin() ) {
                    if ( *std::prev( seg.value ) == val ) {
                        *seg.begin += one; // merge with left neighbour
                        return;
                    }
                }

                _bounds->insert( seg.end, idx + one );
                _values->insert( seg.value, val );
            } else if( idx.symbolic == seg.end->symbolic - sym::constant( 1 ) ) {
                // rewrite last character of a segment
                if ( seg.end != std::prev( _bounds->end() ) ) {
                    if ( *std::next( seg.value ) == val ) {
                        *seg.end -= one; // merge with right neighbour
                        return;
                    }
                }

                _values->insert( std::next( seg.value ), val );
                _bounds->insert( seg.end, idx );
            } else {
                // rewrite segment in the middle = split a segment
                auto vit = _values->insert( seg.value, *seg.value );
                _values->insert( std::next( vit ), val );
                auto bit = _bounds->insert( seg.end, idx );
                _bounds->insert( std::next( bit ), idx + one );
            }
        }

        //_LART_INLINE
        TAbstract size() const noexcept
        {
            return empty() ? sym::constant( 0 ) : end().symbolic;
        }

        View interest() const noexcept
        {
            auto seg = segment( _offset.symbolic );

            auto term = seg;
            while ( term.value != _values->end() && !term.has_value( '\0' ) ) {
                ++term;
            }
            assert( term.value != _values->end() && "missing string of interest" );
            return { _offset, range( seg.begin, term.begin ), range( seg.value, term.value ) };
        }

        TBound _offset;
        std::shared_ptr< Values > _values;
        std::shared_ptr< Bounds > _bounds;
    };

    template< typename Split >
    size_t strlen( const Split * split ) noexcept
    {
        auto interest = split->interest();
        auto seg = interest.front();

        while ( !seg.has_value( '\0' ) ) {
            ++seg;
        }

        auto len = seg.begin->symbolic - interest.offset.symbolic;
        auto interval = seg.begin->interval - interest.offset.interval;
        return lower( len, interval );
    }

    template< typename Split >
    int strcmp( const Split * lhs, const Split * rhs ) noexcept
    {
        const auto & li = lhs->interest();
        const auto & ri = rhs->interest();

        auto le = li.front();
        auto re = ri.front();

        auto from_offset = [] ( const auto & idx, const auto & offset ) {
            return (idx - offset).symbolic;
        };

        while ( le.begin != li.bounds.end() && re.begin != ri.bounds.end() ) {
            if ( *le.value != *re.value ) {
                return *le.value - *re.value;
            } else {
                if ( from_offset( *le.end, li.offset ) > from_offset( *re.end, ri.offset ) ) {
                    return *le.value - *( ++re ).value;
                } else if ( from_offset( *le.end, li.offset ) < from_offset( *re.end, ri.offset ) ) {
                    return *( ++le ).value - *re.value;
                }
            }

            while ( le.empty() && le.begin != li.bounds.end() )
                ++le;
            while ( re.empty() && le.begin != ri.bounds.end() )
                ++re;
        }

        return 0;
    }

    template< typename Split >
    Split * strcpy( const Split * /*dst*/, const Split * /*src*/ ) noexcept
    {
        _UNREACHABLE_F( "Not implemented" );
    }

    template< typename Split >
    Split * strcat( const Split * /*dst*/, const Split * /*src*/ ) noexcept
    {
        _UNREACHABLE_F( "Not implemented" );
    }

    template< typename Split >
    Split * strchr( const Split * str, char ch ) noexcept
    {
        auto interest = str->interest();
        auto seg = interest.front();

        while ( seg.value != interest.values.end() ) {
            if ( seg.has_value( ch ) ) {
                auto offset = *seg.begin;
                if ( offset.symbolic < interest.offset.symbolic )
                    offset = interest.offset;
                return __new< Split >( _VM_PT_Heap, offset, str->_values, str->_bounds );
            }
            ++seg;
        }

        return nullptr;
    }

    namespace sym {

        using Split = Segmentation< ConstrainedValue, char >;

        template< typename... Segments >
        //_LART_INLINE
        Split segmentation( const Segments &... segments ) noexcept
        {
            auto split = Split();
            ( split.append( segments ), ... );
            return split;
        }
    } // namespace sym

} // namespace abstract::mstring
