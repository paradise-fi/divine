#include <rst/split.h>

namespace abstract::mstring {

    namespace {
        auto split( Segment & s, int idx ) -> std::pair< Segment, Segment >
        {
            return { Segment{ s.from, idx, s.value }, Segment{ idx + 1, s.to, s.value } };
        };

        void merge_neighbours( Section * sec, Segment * seg ) noexcept
        {
            auto next = seg->to;
            if ( next < sec->to() ) {
                auto * right = std::next( seg );
                if ( right->value == seg->value ) {
                    seg->to = right->to;
                    sec->erase( right );
                }
            }

            auto prev = seg->from - 1;
            if ( prev >= sec->from() ) {
                auto * left = std::prev( seg );
                if ( left->value == seg->value ) {
                    seg->from = left->from;
                    sec->erase( left );
                }
            }

        }

        inline int begin_value( const Section * sec ) noexcept
        {
            if ( !sec )
                return 0;
            return sec->segments().front().value;
        }

        inline int value( const Section * sec, const Segment * seg ) noexcept
        {
            if ( seg != sec->segments().end() )
                return seg->value;
            return 0;
        }

        Section * get_boundary_section( Sections & secs, int bound ) noexcept
        {
            auto ovewrites = [bound] ( const Section * sec ) {
                return sec->to() - 1 <= bound;
            };

            auto * sec = secs.begin();
            while ( sec != secs.end() && ovewrites( sec ) ) {
                ++sec;
            }
            return sec;
        };

    } // anonymous namespace

    void Section::append( Segment && seg ) noexcept
    {
        _segments.emplace_back( std::move( seg ) );
        merge_neighbours( this, std::prev( _segments.end() ) );
    }

    void Section::prepend( Segment && seg ) noexcept
    {
        _segments.insert( _segments.begin(), std::move( seg ) );
        merge_neighbours( this, _segments.begin() );
    }

    void Section::drop( int bound ) noexcept
    {
        if ( _segments.size() == 1 ) {
            _segments[ 0 ].from = bound;
        } else {
            auto beg = _segments.begin();
            while ( beg->to <= bound )
                ++beg;
            beg->from = bound;
            _segments.erase( _segments.begin(), beg );
        }
    }

    void Section::merge( const Section * sec ) noexcept
    {
        auto mid = _segments.size() - 1;
        auto & segs = sec->segments();
        _segments.append( segs.size(), segs.begin(), segs.end() );
        merge_neighbours( this, &_segments[ mid ] );
    }

    Segment * Section::erase( Segment * seg ) noexcept
    {
        return _segments.erase( seg );
    }

    Segment * Section::erase( Segment * from, Segment * to ) noexcept
    {
        return _segments.erase( from, to );
    }

    const Section * Split::interest() const noexcept
    {
        assert( well_formed() );
        if ( empty() )
            return nullptr;

        const auto & front = sections().front();
        if ( front.from() == 0 )
            return &front;
        return nullptr;
    }

    Section * Split::interest() noexcept
    {
        return const_cast< Section * >( const_cast< const Split * >( this )->interest() );
    }

    bool Split::well_formed() const noexcept
    {
        return this->empty() || sections().front().to() < size();
    }

    void Split::strcpy( const Split * other ) noexcept
    {
        if ( this != other ) {
            auto src = other->interest();

            if ( !src ) {
                if ( auto sec = sections().begin(); sec != sections().end() ) {
                    sec->drop( 1 ); // rewrite first character by zero
                }
                return;
            }

            auto & secs = sections();
            auto slen = src->size();

            assert( slen <= size() && "copying mstring to smaller mstring" );
            auto * bound = get_boundary_section( secs, slen );

            if ( bound == secs.end() ) {
                secs.clear();
                secs.push_back( *src );
            } else {
                if ( bound != secs.begin() ) {
                    bound = std::prev( secs.erase( secs.begin(), bound ) );
                }

                if ( bound != secs.end() && bound->from() <= src->to() ) {
                    bound->drop( slen + 1 );
                }

                secs.insert( secs.begin(), *src );
            }
        }
    }

    void Split::strcat( const Split * other ) noexcept
    {
        auto left = interest();
        // TODO get rid of unnecessary copy
        Section right = *other->interest();

        int begin = left ? left->size() : 0;
        int dist = right.size() + 1;
        int end = begin + dist;
        assert( size() >= end && "concating mstring into smaller mstring" );

        auto & secs = sections();

        // concat sections
        if ( left ) {
            int shift = left->to();
            for ( auto & seg : right.segments() ) {
                seg.from +=  shift;
                seg.to += shift;
            }

            left->merge( &right );
        } else {
            if ( secs.empty() ) {
                secs.push_back( right );
                left = secs.begin();
            } else {
                left = secs.insert( secs.begin(), right );
            }
        }

        // drop overwritten sections
        auto it = std::next( left );
        while ( it != secs.end() && it->from() <= end ) {
            if ( it->to() <= end ) {
                it = std::prev( secs.erase( it ) );
            } else {
                it->drop( end );
                break;
            }
        }
    }

    Split * Split::strchr( char /*ch*/ ) const noexcept
    {
        /*auto str = interest();

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
        }*/
        _UNREACHABLE_F( "Not implemented." );
    }

    int Split::strlen() const noexcept
    {
        if ( empty() )
            return 0;
        if ( auto * str = interest() )
            return str->size();
        return 0;
    }

    int Split::strcmp( const Split * other ) const noexcept
    {
        const auto * lhs = interest();
        const auto * rhs = other->interest();

        if ( !lhs || !rhs )
            return begin_value( lhs ) - begin_value( rhs );

        const auto & lsegs = lhs->segments();
        const auto & rsegs = rhs->segments();

        auto * lseg = lsegs.begin();
        auto * rseg = rsegs.begin();

        while ( lseg != lsegs.end() && rseg != rsegs.end() ) {
            char lhsv = lseg->value;
            char rhsv = rseg->value;

            if ( lhsv != rhsv ) {
                return lhsv - rhsv;
            } else {
                if ( lseg->to > rseg->to ) {
                    return lhsv - value( rhs, std::next( rseg ) );
                } else if ( lseg->to < rseg->to ) {
                    return value( lhs, std::next( lseg ) ) - rhsv;
                }
            }

            ++lseg;
            ++rseg;
        }

        return value( lhs, lseg ) - value( rhs, rseg );
    }

    Section * Split::section_of( int idx ) noexcept
    {
        return const_cast< Section * >( const_cast< const Split * >( this )->section_of( idx ) );
    }

    const Section * Split::section_of( int idx ) const noexcept
    {
        for ( const auto& sec : sections() ) {
            if ( idx >= sec.from() && idx < sec.to() )
                return &sec;
            if ( idx < sec.from() )
                return nullptr;
        }

        return nullptr;
    }

    void Split::shrink_correction( Section * sec, Segment * bound ) noexcept
    {
        if ( sec->empty() ) {
            sections().erase( sec );
        } else {
            if ( bound->empty() )
                sec->erase( bound );
        }
    }

    void Split::shrink_left( Section * sec, Segment * bound ) noexcept
    {
        bound->from++;
        shrink_correction( sec, bound );
    }

    void Split::shrink_right( Section * sec, Segment * bound ) noexcept
    {
        bound->to--;
        shrink_correction( sec, bound );
    }

    void Split::divide( Section * sec, Segment & seg, int idx ) noexcept
    {
        auto [left, right] = split( seg, idx );

        Section right_section;
        if ( !right.empty() )
            right_section.append( std::move( right ) );
        for ( auto rest = std::next( &seg ); rest != sec->segments().end(); ++rest )
            right_section.append( std::move( *rest ) );

        sec->erase( &seg, sec->segments().end() );
        if ( !left.empty() )
            sec->append( std::move( left ) );

        sections().insert( std::next( sec ), right_section );
    }

    void Split::write_zero( int idx ) noexcept
    {
        auto sec = section_of( idx );
        if ( !sec )
            return; // position already contains zero

        auto & seg = sec->segment_of( idx );

        if ( idx == sec->from() ) {
            shrink_left( sec, &seg );
        } else if ( idx == sec->to() - 1 ) {
            shrink_right( sec, &seg );
        } else {
            divide( sec, seg, idx );
        }
    }

    void Split::overwrite_char( Section * sec, Segment & seg, int idx, char val ) noexcept
    {
        if ( seg.size() == 1 ) {
            seg.value = val;
            merge_neighbours( sec, &seg );
        } else {
            auto [left, right] = split( seg, idx );
            auto & segs = sec->segments();

            seg.from = idx;
            seg.to = idx + 1;
            seg.value = val;

            auto place = &seg;
            if ( !left.empty() ) {
                auto it = segs.insert( place, left );
                place = std::next( it );
            }

            if ( !right.empty() ) {
                auto it = segs.insert( std::next( place ), right );
                place = std::prev( it );
            }

            if ( left.empty() || right.empty() )
                merge_neighbours( sec, place );
        }
    }

    void Split::overwrite_zero( int idx, char val ) noexcept
    {
        auto left = section_of( idx - 1 );
        auto right = section_of( idx + 1 );

        Segment seg{ idx, idx + 1, val };

        auto & secs = sections();

        if ( left && right ) { // merge sections
            left->append( std::move( seg ) );
            left->merge( right );
            secs.erase( right );
        } else if ( left ) {
            left->append( std::move( seg ) );
        } else if ( right ) { // extend right section
            right->prepend( std::move( seg ) );
        } else { // create a new section
            Section new_section;
            new_section.append( std::move( seg ) );

            if ( secs.empty() ) {
                secs.push_back( new_section );
            } else {
                auto point = secs.begin();
                while ( point->to() < idx ) ++point;
                secs.insert( point, new_section );
            }
        }
    }

    void Split::write_char( int idx, char val ) noexcept
    {
        if ( auto sec = section_of( idx ) ) {
            auto & seg = sec->segment_of( idx );
            if ( seg.value == val )
                return;
            overwrite_char( sec, seg, idx, val );
        } else {
            overwrite_zero( idx, val );
        }
    }

    void Split::write( int idx, char val ) noexcept
    {
        assert( idx < size() ); // TODO consider offset
        if ( val == '\0' )
            write_zero( idx );
        else
            write_char( idx, val );
    }


    char Split::read( int idx ) noexcept
    {
        assert( idx < size() ); // TODO consider offset
        if ( auto sec = section_of( idx ) )
            return sec->segment_of( idx ).value;
        return '\0';
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
