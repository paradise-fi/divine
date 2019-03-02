#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <rst/domains.h>
#include <rst/common.h>
#include <rst/lart.h>

#include <util/array.hpp>

namespace abstract::mstring {

    struct Mstring;

    /*
     *  |      section      |
     *  |  segment  |
     *  | a | a | a | b | b | \0 | c | c |
     */

    struct Segment {
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


    template< typename Buffer >
    struct Section {
        explicit Section( const Buffer& buff, size_t from, size_t to )
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
            assert( idx < size() );

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

    template< typename Buffer >
    using Sections = __dios::Array< Section< Buffer > >;


    template< typename Buffer >
    struct Split {
        Split( const Buffer& buff, const __dios::Array< size_t >& terminators )
        {
            unsigned preffix = 0;
            for ( auto terminator : terminators ) {
                _sections.push_back( Section( buff, preffix, terminator ) );
                preffix = terminator + 1;
            }

            // last substring
            if ( preffix < buff.size() )
                _sections.push_back( Section( buff, preffix, buff.size() ) );
        }

        const Sections< Buffer >& sections() const noexcept { return _sections; }

    private:
        Sections< Buffer > _sections;
    };


    template< typename T >
    struct Buffer {
        Buffer( size_t from, const T * buff, size_t len )
            : _from( from ), _len( len )
        {
            _buff = reinterpret_cast< char * >( __vm_obj_make( _len, _VM_PT_Heap ) );
            std::memcpy( _buff, buff, _len );
        }

        Buffer( size_t from, const Buffer & other )
            : Buffer( from, other.data(), other.size() )
        {}

        ~Buffer() { __vm_obj_free( _buff ); }

        constexpr size_t size() const noexcept { return _len; }

        T& operator[] ( int idx ) noexcept { return _buff[ idx ]; }
        const T& operator[] ( int idx ) const noexcept { return _buff[ idx ]; }

        constexpr T* data() noexcept { return _buff; }
        constexpr const T* data() const noexcept { return _buff; }

        constexpr bool empty() const noexcept { return _len == 0; }

        constexpr size_t from() const noexcept { return _from; }
    private:
        const size_t _from;
        const size_t _len;
        T * _buff;
    };

    struct Mstring {
        using Buffer = Buffer< char >;

        Mstring( const char * buff, size_t len, size_t from, size_t refcount = 0 )
            : _buff( from, buff, len ), _from( from ), _refcount( refcount )
        {
            init();
        }

        Mstring( Mstring * str, size_t from )
            : _buff( from, str->_buff ), _from( from )
        {
            init();
        }

        char * rho() const;
        Split< Buffer > split() const noexcept;

        constexpr size_t from() const noexcept { return _buff.from(); }

        size_t strlen() const noexcept;
        int strcmp( const Mstring * other ) const noexcept;

        void strcpy( const Mstring * other ) noexcept;
        void strcat( const Mstring * other ) noexcept;

        Mstring * strchr( char ch ) const noexcept;

        void write( size_t idx, char val ) noexcept;
        void safe_write( size_t idx, char val ) noexcept;

        char read( size_t idx ) noexcept;

        constexpr char * data() noexcept { return _buff.data(); }
        constexpr const char * data() const noexcept { return _buff.data(); }

        size_t refcount() const noexcept { return _refcount; }
        void refcount_decrement() { --_refcount; }
        void refcount_increment() { ++_refcount; }

    private:

        void init() noexcept {
            for ( size_t i = _from; i < _buff.size(); ++i ) {
                if ( _buff[i] == '\0' )
                    _terminators.push_back(i);
            }
            assert(!_terminators.empty());
            assert(_terminators.front() <= _buff.size());
        }

        size_t _from;

        Buffer _buff;                           // IV - buffer
        __dios::Array< size_t > _terminators;   // T - zeros in buffer

        size_t _refcount;
    };

    void mstring_release( Mstring * str ) noexcept;

    void mstring_cleanup( Mstring * str ) noexcept;
    void mstring_cleanup_check( Mstring * str ) noexcept;

    using Release = decltype( mstring_release );
    using Check = decltype( mstring_cleanup_check );
} // namespace abstract::mstring

typedef abstract::mstring::Mstring __mstring;

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
    using Object = abstract::mstring::Mstring;
    using Release = abstract::mstring::Release;
    using Check = abstract::mstring::Check;

    // explicitly instantiate because of __invisible attribute
    template __invisible void cleanup< Object, Release, Check >(
        _VM_Frame * frame, Release release, Check check
    ) noexcept;
} // namespace abstract

namespace abstract::mstring {
    __invisible static inline void cleanup( _VM_Frame * frame ) noexcept {
        abstract::cleanup< Mstring, Release, Check >( frame, mstring_release, mstring_cleanup_check );
    }
} // namespace abstract::mstring

