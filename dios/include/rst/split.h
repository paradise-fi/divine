#pragma once

#include <rst/common.h>
#include <util/array.hpp>

#include <assert.h>

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
            : from( from ), to( to ), value( val )
        {}

        _LART_INLINE
        size_t size() const noexcept { return to - from; }

        _LART_INLINE
        bool empty() const noexcept { return size() == 0; }

        _LART_INLINE
        auto divide( size_t idx ) -> std::pair< Segment, Segment >
        {
            return { Segment{ from, idx, value }, Segment{ idx + 1, to, value } };
        }

        void dump() const noexcept
        {
            __dios_trace_f( "seg: [%lu, %lu) = %c", from, to, value );
        }

        size_t from, to;
        char value;
    };

    inline bool operator==( const Segment& lhs, const Segment& rhs)
    {
        return std::tie( lhs.from, lhs.to, lhs.value ) == std::tie( rhs.from, rhs.to, rhs.value );
    }

    struct Section : Array< Segment >
    {
        _LART_INLINE
        size_t length() const noexcept
        {
            return this->empty() ? 0 : to() - from();
        }

        _LART_INLINE
        size_t from() const noexcept { return front().from; }

        _LART_INLINE
        size_t to() const noexcept { return back().to; }

        void start_from( size_t idx ) noexcept;

        void append( Segment && seg ) noexcept;
        void prepend( Segment && seg ) noexcept;

        void merge( const Section * sec ) noexcept;
        void merge_neighbours( iterator seg ) noexcept;

        void drop_front( size_t to ) noexcept;
        void drop_back( size_t from ) noexcept;
        void drop( size_t from, size_t to ) noexcept;

        auto view( size_t from, size_t to ) noexcept;

        _LART_INLINE
        const Segment& segment_of( size_t idx ) const noexcept
        {
            assert( idx >= from() && idx < to() );
            for ( const auto & seg : *this ) {
                if ( idx >= seg.from && idx < seg.to )
                    return seg;
            }
        }

        _LART_INLINE
        Segment& segment_of( size_t idx ) noexcept
        {
            return const_cast< Segment & >(
                const_cast< const Section * >( this )->segment_of( idx )
            );
        }

        _LART_INLINE
        char value( const_iterator seg ) const noexcept
        {
            if ( seg != end() )
                return seg->value;
            return '\0';
        }

        _LART_INLINE
        void dump() const noexcept
        {
            __dios_trace_f( "section: [%lu, %lu)", from(), to() );
            for ( const auto & seg : *this )
                seg.dump();
        }
    };

    _LART_INLINE
    static Section make_section( const char * buff, size_t from, size_t to )
    {
        Section sec;

        char c = buff[ from ];
        for ( size_t i = from; i < to; ) {
            size_t j = i + 1;
            while ( j < to && buff[j] == c ) { ++j; }
            sec.push_back( Segment( i, j, c ) );
            i = j;
            if ( i < to )
                c = buff[ i ];
        }

        return sec;
    }


    using Sections = Array< Section >;

    struct SplitData
    {
        size_t len;
        Sections sections;
    };

    struct Split
    {
        Split( const char * buff, size_t len )
            : _offset( 0 ), _refcount( 0 )
        {
            if ( buff == nullptr )
                assert( len == 0 );

            _data = std::make_shared< SplitData >();
            _data->len = len;

            bool seen_zero = true;
            size_t from = 0;
            for ( size_t i = 0; i < len; ++i ) {
                if ( seen_zero && buff[ i ] != '\0' ) {
                    seen_zero = false;
                    from = i;
                } else if ( !seen_zero && buff[ i ] == '\0' ) {
                    _data->sections.push_back( make_section( buff, from, i ) );
                    seen_zero = true;
                    from = i + 1;
                }
            }

            // last substring
            if ( !seen_zero && from < len )
                _data->sections.push_back( make_section( buff, from, len ) );
        }

        Split( std::shared_ptr< SplitData > data )
            : _offset( 0 ), _refcount( 0 ), _data( data )
        {}

        const Section * interest() const noexcept;
        Section * interest() noexcept;

        Section * section_of( size_t idx ) noexcept;
        const Section * section_of( size_t idx ) const noexcept;

        bool empty() const noexcept { return _data->sections.empty(); }
        bool well_formed() const noexcept;

        operator bool() const noexcept { return !empty(); }

        size_t strlen() const noexcept;
        int strcmp( const Split * other ) const noexcept;

        void strcpy( const Split * other ) noexcept;
        void strcat( const Split * other ) noexcept;
        Split * strchr( char ch ) const noexcept;

        void write( size_t idx, char val ) noexcept;
        char read( size_t idx ) const noexcept;

        Split * offset( size_t idx ) const noexcept;

        size_t getOffset() const noexcept { return _offset; }

        // size of underlying buffer
        size_t size() const noexcept { return _data->len; }
        void extend( size_t size ) const noexcept
        {
            assert( this->size() < size );
            _data->len = size;
            // TODO how to set uninitialized values?
        }

        void drop( size_t from, size_t to ) noexcept;

        Sections & sections() noexcept { return _data->sections; }
        const Sections & sections() const noexcept { return _data->sections; }

        size_t refcount() const noexcept { return _refcount; }
        void refcount_decrement() { --_refcount; }
        void refcount_increment() { ++_refcount; }

        void dump() const noexcept {
            __dios_trace_f( "split" );
            for ( const auto & sec : _data->sections )
                sec.dump();
        }
    private:
        void write_zero( size_t idx ) noexcept;
        void write_char( size_t idx, char val ) noexcept;

        void shrink_left( Section * sec, Segment * bound ) noexcept;
        void shrink_right( Section * sec, Segment * bound ) noexcept;
        void shrink_correction( Section * sec, Segment * bound ) noexcept;

        void overwrite_char( Section * sec, Segment & seg, size_t idx, char val ) noexcept;
        void overwrite_zero( size_t idx, char val ) noexcept;

        void divide( Section * sec, Segment & seg, size_t idx ) noexcept;

        size_t _refcount;
        size_t _offset;

        std::shared_ptr< SplitData > _data;
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

