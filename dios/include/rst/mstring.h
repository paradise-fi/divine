#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <rst/domains.h>
#include <rst/common.h>
#include <rst/lart.h>

#include <util/array.hpp>

namespace abstract::mstring {

    struct Split;

    /*
     *  |      section      |    |  sec. |
     *  |  segment  |
     *  | a | a | a | b | b | \0 | c | c |
     */

    struct Segment
    {
        explicit Segment( size_t from, size_t to, char val )
            : _from( from ), _to( to ), _val( val )
        {}

        constexpr size_t size() const noexcept { return _to - _from; }
        constexpr size_t from() const noexcept { return _from; }
        constexpr size_t to() const noexcept { return _to; }

        char value() const noexcept { return _val; }

    private:
        size_t _from, _to;
        char _val;
    };

    using Segments = __dios::Array< Segment >;

    struct Section
    {
        explicit Section( const char * buff, size_t from, size_t to )
            : _from( from ), _to( to )
        {
            char c = buff[ _from ];
            for ( size_t i = from; i < to; ) {
                size_t j = i + 1;
                while ( j < to && buff[j] == c ) { ++j; }
                _segments.push_back( Segment( i, j, c ) );
                i = j;
            }
        }

        constexpr bool empty() const noexcept { return _segments.empty(); }

        constexpr size_t size() const noexcept { return _to - _from; }
        size_t from() const noexcept { return _from; }
        size_t to() const noexcept { return _to; }

        const Segments& segments() const noexcept { return _segments; }

        const Segment& segment_of( size_t idx ) const noexcept {
            ASSERT( idx < size() );

            int bound = 0;
            for ( const auto& seg : _segments ) {
                bound += seg.size();
                if ( idx < bound )
                    return seg;
            }
        }

        char operator[] ( int idx ) const noexcept {
            return segment_of( idx ).value();
        }

    private:
        size_t _from, _to;
        Segments _segments;
    };

    using Sections = __dios::Array< Section >;

    struct Split
    {
        Split( const char * buff, size_t len )
            : _len( len )
        {
            if ( buff == nullptr )
                ASSERT( len == 0 );

            int from = 0;
            for ( int i = 0; i < len; ++i ) {
                if ( buff[ i ] == '\0' ) {
                    _sections.push_back( Section( buff, from, i ) );
                    from = i + 1;
                }
            }

            // last substring
            if ( from < len )
                _sections.push_back( Section( buff, from, len ) );
        }

        Section & interest() noexcept;
        const Section & interest() const noexcept;

        Section & section_of( int idx ) noexcept;
        const Section & section_of( int idx ) const noexcept;

        bool empty() const noexcept { return _sections.empty(); }
        bool well_formed() const noexcept;

        size_t strlen() const noexcept;
        int strcmp( const Split * other ) const noexcept;

        void strcpy( const Split * other ) noexcept;
        void strcat( const Split * other ) noexcept;

        Split * strchr( char ch ) const noexcept;

        void write( size_t idx, char val ) noexcept;
        char read( size_t idx ) noexcept;

        size_t size() const noexcept { return _len; }

        size_t refcount() const noexcept { return _refcount; }
        void refcount_decrement() { --_refcount; }
        void refcount_increment() { ++_refcount; }
    private:
        size_t _len;
        Sections _sections;
        size_t _refcount;
    };

    void split_release( Split * str ) noexcept;
    void split_cleanup( Split * str ) noexcept;
    void split_cleanup_check( Split * str ) noexcept;

    using Release = decltype( split_release );
    using Check = decltype( split_cleanup_check );
} // namespace abstract::mstring

typedef abstract::mstring::Split __mstring;

#define DOMAIN_NAME mstring
#define DOMAIN_KIND content
#define DOMAIN_TYPE __mstring *
    #include <rst/string-domain.def>
#undef DOMAIN_NAME
#undef DOMAIN_TYPE
#undef DOMAIN_KIND

extern "C" {
    __mstring * __mstring_undef_value();
}

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
