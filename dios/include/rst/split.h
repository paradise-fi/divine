#pragma once

#include <rst/common.h>
#include <util/array.hpp>

namespace abstract::mstring {

    struct Split;

    /*
     *  |      section      |    |  sec. |
     *  |  segment  |
     *  | a | a | a | b | b | \0 | c | c |
     */

    template< typename T >
    using Array = __dios::Array< T >;

    struct Segment
    {
        explicit Segment( int from, int to, char val )
            : from( from ), to( to ), value( val )
        {}

        int size() const noexcept { return to - from; }
        bool empty() const noexcept { return size() == 0; }

        void dump() const noexcept
        {
            __dios_trace_f( "seg: [%d, %d) = %c", from, to, value );
        }

        int from, to;
        char value;
    };

    inline bool operator==( const Segment& lhs, const Segment& rhs)
    {
        return std::tie( lhs.from, lhs.to, lhs.value ) == std::tie( rhs.from, rhs.to, rhs.value );
    }

    using Segments = Array< Segment >;

    struct Section
    {
        explicit Section( const char * buff, int from, int to )
        {
            char c = buff[ from ];
            for ( int i = from; i < to; ) {
                int j = i + 1;
                while ( j < to && buff[j] == c ) { ++j; }
                _segments.push_back( Segment( i, j, c ) );
                i = j;
                if ( i < to )
                    c = buff[ i ];
            }
        }

        Section() = default;
        Section(const Section&) = default;
        Section(Section&&) = default;

        bool empty() const noexcept { return size() == 0; }

        int size() const noexcept { return to() - from(); }
        int from() const noexcept { return _segments.front().from; }
        int to() const noexcept { return _segments.back().to; }

        const Segments& segments() const noexcept { return _segments; }
        Segments& segments() noexcept { return _segments; }

        Segment * erase( Segment * seg ) noexcept;
        Segment * erase( Segment * from, Segment * to ) noexcept;

        void append( Segment && seg ) noexcept;
        void prepend( Segment && seg ) noexcept;

        void merge( const Section * sec ) noexcept;

        void drop( int bound ) noexcept;

        const Segment& segment_of( int idx ) const noexcept {
            assert( idx >= from() && idx < to() );
            for ( const auto & seg : _segments ) {
                if ( idx >= seg.from && idx < seg.to )
                    return seg;
            }
        }

        Segment& segment_of( int idx ) noexcept {
            return const_cast< Segment & >(
                const_cast< const Section * >( this )->segment_of( idx )
            );
        }

    private:
        Segments _segments;
    };

    using Sections = Array< Section >;

    struct SplitData
    {
        int len;
        Sections sections;
    };

    struct Split
    {
        Split( const char * buff, int len )
        {
            if ( buff == nullptr )
                assert( len == 0 );

            _data = __new< SplitData >( _VM_PT_Weak );
            _data->len = len;

            bool seen_zero = true;
            int from = 0;
            for ( int i = 0; i < len; ++i ) {
                if ( seen_zero && buff[ i ] != '\0' ) {
                    seen_zero = false;
                    from = i;
                } else if ( !seen_zero && buff[ i ] == '\0' ) {
                    _data->sections.push_back( Section( buff, from, i ) );
                    seen_zero = true;
                    from = i + 1;
                }
            }

            // last substring
            if ( !seen_zero && from < len )
                _data->sections.push_back( Section( buff, from, len ) );
        }

        ~Split()
        {
            __dios_safe_free( _data );
        }

        const Section * interest() const noexcept;
        Section * interest() noexcept;

        Section * section_of( int idx ) noexcept;
        const Section * section_of( int idx ) const noexcept;

        bool empty() const noexcept { return _data->sections.empty(); }
        bool well_formed() const noexcept;

        int strlen() const noexcept;
        int strcmp( const Split * other ) const noexcept;

        void strcpy( const Split * other ) noexcept;
        void strcat( const Split * other ) noexcept;
        Split * strchr( char ch ) const noexcept;

        void write( int idx, char val ) noexcept;
        char read( int idx ) noexcept;

        int size() const noexcept { return _data->len; }

        Sections & sections() noexcept { return _data->sections; }
        const Sections & sections() const noexcept { return _data->sections; }

        int refcount() const noexcept { return _refcount; }
        void refcount_decrement() { --_refcount; }
        void refcount_increment() { ++_refcount; }
    private:
        void write_zero( int idx ) noexcept;
        void write_char( int idx, char val ) noexcept;

        void shrink_left( Section * sec, Segment * bound ) noexcept;
        void shrink_right( Section * sec, Segment * bound ) noexcept;
        void shrink_correction( Section * sec, Segment * bound ) noexcept;

        void overwrite_char( Section * sec, Segment & seg, int idx, char val ) noexcept;
        void overwrite_zero( int idx, char val ) noexcept;

        void divide( Section * sec, Segment & seg, int idx ) noexcept;

        int _refcount;
        SplitData * _data;
    };

    void split_release( Split * str ) noexcept;
    void split_cleanup( Split * str ) noexcept;
    void split_cleanup_check( Split * str ) noexcept;

    using Release = decltype( split_release );
    using Check = decltype( split_cleanup_check );
} // namespace abstract::mstring

namespace abstract {
    using Object = abstract::mstring::Split;
    using Release = abstract::mstring::Release;
    using Check = abstract::mstring::Check;

    // explicitly instantiate because of __invisible attribute
    template __invisible void cleanup< Object, Release, Check >(
        _VM_Frame * frame, Release release, Check check
    ) noexcept;
} // namespace abstract

namespace abstract::mstring {
    __invisible static inline void cleanup( _VM_Frame * frame ) noexcept {
        abstract::cleanup< Object, Release, Check >( frame,
            split_release, split_cleanup_check
        );
    }
} // namespace abstract::mstring

