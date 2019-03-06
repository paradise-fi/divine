#include <rst/split.h>

namespace abstract::mstring {

    Section & Split::interest() noexcept
    {
        ASSERT( well_formed() );
        return _sections.front();
    }

    const Section & Split::interest() const noexcept
    {
        ASSERT( well_formed() );
        return _sections.front();
    }

    bool Split::well_formed() const noexcept
    {
        return this->empty() || _sections.front().to() < _len;
    }

    void Split::strcpy( const Split * other ) noexcept
    {
        if ( this != other ) {
            auto str = other->interest();
            // TODO check correct bounds
            ASSERT( str.size() < _len && "copying mstring to smaller mstring" );
            // TODO copy segments
            _UNREACHABLE_F( "Not implemented." );
        }
    }

    void Split::strcat( const Split * other ) noexcept
    {
        size_t begin = interest().size();
        size_t dist = other->interest().size() + 1;

        // TODO check correct bounds
        ASSERT( _len >= begin + dist && "concating mstring into smaller mstring" );
        // TODO copy segments
        _UNREACHABLE_F( "Not implemented." );
    }

    Split * Split::strchr( char ch ) const noexcept
    {
        auto str = interest();

        if ( !str.empty() ) {
            const auto &begin = str.segments().begin();
            const auto &end = str.segments().end();

            auto search = std::find_if( begin, end, [ch] ( const auto &seg ) {
                return seg.value() == ch;
            });

            if ( search != end ) {
                // TODO return subsplit
                // return __new< Split >( _VM_PT_Marked, const_cast< Split * >( this ), search->from() );
                _UNREACHABLE_F( "Not implemented." );
            } else {
                return nullptr;
            }
        } else {
            _UNREACHABLE_F("Error: there's no string of interest!");
        }
    }

    size_t Split::strlen() const noexcept
    {
        if ( empty() )
            return 0;
        return interest().size();
    }

    int Split::strcmp( const Split * other ) const noexcept
    {
        const auto & sq1 = interest();
        const auto & sq2 = other->interest();

        if ( !sq1.empty() && !sq2.empty() ) {
            for ( size_t i = 0; i < sq1.size(); i++ ) {
                const auto& sq1_seg = sq1.segment_of( i ); // TODO optimize segment_of
                const auto& sq2_seg = sq2.segment_of( i );

               // TODO optimize per segment comparison
                if ( sq1_seg.value() == sq2_seg.value() ) {
                    if ( sq1_seg.to() > sq2_seg.to() ) {
                        return sq1_seg.value() - sq2.segment_of( i + 1 ).value();
                    } else if (sq1_seg.to() < sq2_seg.to() ) {
                        return sq1.segment_of( i + 1 ).value() - sq2_seg.value();
                    }
                } else {
                    return sq1[ i ] - sq2[ i ];
                }
            }

            return 0;
        }

        _UNREACHABLE_F( "Error: there is no string of interest." );
    }

    Section & Split::section_of( int /*idx*/ ) noexcept
    {
        _UNREACHABLE_F( "Not implemented." );
    }

    const Section & Split::section_of( int /*idx*/ ) const noexcept
    {
        _UNREACHABLE_F( "Not implemented." );
    }

    void Split::write( size_t idx, char val ) noexcept
    {
        ASSERT( idx < _len );
        // TODO check value
        // if ( _buff[ idx ] == '\0' ) {
            // TODO merge sections
        // }
        // TODO change value
        //_buff[ idx ] = val;
        if ( val == '\0' ) {
            // TODO split sections
        }

        _UNREACHABLE_F( "Not implemented." );
    }

    char Split::read( size_t idx ) noexcept
    {
        ASSERT( idx < _len );
        _UNREACHABLE_F( "Not implemented." );
    }

    void split_release( Split * str ) noexcept
    {
        str->refcount_decrement();
    }

    void split_cleanup( Split * str ) noexcept
    {
        str->~Split();
        __dios_safe_free( str );
    }

    void split_cleanup_check( Split * str ) noexcept
    {
        cleanup_check( str, split_cleanup );
    }

} // namespace abstract::mstring
