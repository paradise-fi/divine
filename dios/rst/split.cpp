#include <rst/split.h>

namespace abstract::mstring {

    namespace {

        _LART_INLINE
        char begin_value( const Section * sec, size_t offset ) noexcept
        {
            if ( !sec )
                return '\0';
            return sec->segment_of( offset ).value;
        }

    } // anonymous namespace

    _LART_INLINE
    void Section::start_from( size_t idx ) noexcept
    {
        for ( auto & seg : *this ) {
            auto size = seg.size();
            seg.from = idx;
            idx += size;
            seg.to = idx;
        }
    }

    _LART_INLINE
    void Section::append( Segment && seg ) noexcept
    {
        emplace_back( std::move( seg ) );
        merge_neighbours( std::prev( end() ) );
    }

    _LART_INLINE
    void Section::prepend( Segment && seg ) noexcept
    {
        insert( begin(), std::move( seg ) );
        merge_neighbours( begin() );
    }

    _LART_INLINE
    void Section::drop_front( size_t to ) noexcept
    {
        drop( from(), to );
    }

    _LART_INLINE
    void Section::drop_back( size_t from ) noexcept
    {
        drop( from, to() );
    }

    _LART_INLINE
    void Section::drop( size_t from, size_t to ) noexcept
    {
        auto beg = begin();
        while ( beg->to < from )
            ++beg;

        if ( beg->from < from )
            beg->to = from;
        else
            beg->from = std::min( beg->to, to );
        auto end = this->end();

        if ( to < this->to() ) {
            end = beg;
            while ( end != this->end() && end->to <= to )
                ++end;
            end->from = to;
        }

        if ( beg == end )
            return;

        if ( !beg->empty() )
            ++beg;

        if ( end != this->end() && end->empty() )
            ++end;

        this->erase( beg, end );
    }

    _LART_INLINE
    void Section::merge( const Section * sec ) noexcept
    {
        assert( size() > 0 );
        auto mid = size() - 1;
        auto * arr = static_cast< Array< Segment > * >( this );
        arr->append( sec->size(), sec->begin(), sec->end() );
        merge_neighbours( std::next( begin(), mid ) );
    }

    _LART_INLINE
    void Section::merge_neighbours( iterator seg ) noexcept
    {
        auto next = seg->to;
        if ( next < to() ) {
            auto * right = std::next( seg );
            if ( right->value == seg->value ) {
                seg->to = right->to;
                erase( right );
            }
        }

        if ( seg->from != 0 ) {
            auto prev = seg->from - 1;
            if ( prev >= from() ) {
                auto * left = std::prev( seg );
                if ( left->value == seg->value ) {
                    seg->from = left->from;
                    erase( left );
                }
            }
        }
    }

    const Section * Split::interest() const noexcept
    {
        assert( well_formed() );
        if ( empty() )
            return nullptr;

        auto sec = sections().begin();
        while ( sec != sections().end() && sec->to() < _offset )
            ++sec;
        if ( sec == sections().end() )
            return nullptr;
        if ( sec->from() <= _offset )
            return sec;
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

    void Split::drop( size_t from, size_t to ) noexcept
    {
        auto & secs = _data->sections;
        auto it = secs.begin();

        //  skip front
        while ( it != secs.end() && it->to() < from )
            ++it;

        if ( it == secs.end() )
            return; // nothing to drop

        if ( to < it->to() ) {
            it->drop( from, to );
        } else {
            if ( from == it->from() ) {
                it = std::prev( secs.erase( it ) );
                if ( secs.empty() || it == secs.end() ) {
                    return;
                }
            } else {
                it->drop_back( from );
                ++it;
            }

            auto end = secs.end();
            if ( to < secs.back().to() ) {
                end = it;
                while ( end != secs.end() && end->to() <= to )
                    ++end;

                if ( end->from() <= to )
                    end->drop_front( to );
            }

            if ( end != secs.end() && end->empty() ) {
                ++end;
            }

            secs.erase( it, end );
        }
    }

    void Split::strcpy( const Split * other ) noexcept
    {
        if ( this != other ) {
            auto src = other->interest();

            Section shifted;

            if ( src ) {
                shifted = *src;
                shifted.drop_front( other->getOffset() );
                shifted.start_from( _offset );
            }

            assert( shifted.length() <= size() - _offset && "copying mstring to smaller mstring" );
            if ( shifted.length() == 0 ) {
                write_zero( _offset );
                return;
            }

            drop( _offset, shifted.length() + _offset + 1 );

            auto sec = section_after_offset();
            insert_section_at( sec, std::move( shifted ) );
        }
    }

    void Split::copy( const Split * src, size_t len ) noexcept
    {
        assert( len <= size() - _offset && "copying mstring to smaller mstring" );
        // TODO check overlap
        assert( len <= src->size() - src->getOffset() &&
                "copying memory from out of src bounds" );

        if ( this != src ) {
            drop( _offset, len );

            auto sec = src->section_after_offset();
            if ( sec != src->sections().end() ) {
                //auto & secs = sections();
                //auto insert = section_after_offset();
                _UNREACHABLE_F( "Not implemented." );
            }
        }
    }

    void Split::strcat( const Split * other ) noexcept
    {
        auto left = interest();
        Section right = *other->interest();

        size_t begin = left ? left->length() : 0;
        size_t dist = other->strlen() + 1;
        size_t end = begin + dist;
        assert( size() >= end && "concating mstring into smaller mstring" );

        auto & secs = sections();

        // concat sections
        if ( left ) {
            right.start_from( left->to() );
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
                it->drop_front( end );
                break;
            }
        }
    }

    Split * Split::strchr( char ch ) const noexcept
    {
        auto sec = interest();
        if ( !sec )
            return nullptr;

        auto seg = sec->begin();
        while ( seg->to <= _offset ) {
            ++seg;
        }

        while ( seg != sec->end() && seg->value != ch )
            ++seg;

        if ( seg != sec->end() ) {
            auto split = __new< Split >( _VM_PT_Heap, _data );
            split->_offset = std::max( seg->from, _offset );
            return split;
        }

        return nullptr;
    }

    size_t Split::strlen() const noexcept
    {
        if ( empty() )
            return 0;
        if ( auto * str = interest() ) {
            return str->to() - _offset;
        }
        return 0;
    }

    int Split::strcmp( const Split * other ) const noexcept
    {
        const auto * lhs = interest();
        const auto * rhs = other->interest();

        auto loff = _offset;
        auto roff = other->getOffset();

        if ( !lhs || !rhs )
            return begin_value( lhs, loff ) - begin_value( rhs, roff );

        auto * lseg = &lhs->segment_of( loff );
        auto * rseg = &rhs->segment_of( roff );

        auto from_offset = [] ( size_t idx, size_t offset ) {
            return idx - offset;
        };

        while ( lseg != lhs->end() && rseg != rhs->end() ) {
            char lhsv = lseg->value;
            char rhsv = rseg->value;

            if ( lhsv != rhsv ) {
                return lhsv - rhsv;
            } else {
                if ( from_offset( lseg->to, loff ) > from_offset( rseg->to, roff ) ) {
                    return lhsv - rhs->value( std::next( rseg ) );
                } else if ( from_offset( lseg->to, loff ) < from_offset( rseg->to, roff ) ) {
                    return lhs->value( std::next( lseg ) ) - rhsv;
                }
            }

            ++lseg;
            ++rseg;
        }

        return lhs->value( lseg ) - rhs->value( rseg );
    }

    Section * Split::section_of( size_t idx ) noexcept
    {
        return const_cast< Section * >( const_cast< const Split * >( this )->section_of( idx ) );
    }

    const Section * Split::section_of( size_t idx ) const noexcept
    {
        for ( const auto& sec : sections() ) {
            if ( idx >= sec.from() && idx < sec.to() )
                return &sec;
            if ( idx < sec.from() )
                return nullptr;
        }

        return nullptr;
    }

    // returns first section that ends after offset
    Section * Split::section_after_offset() noexcept
    {
        return const_cast< Section * >(
            const_cast< const Split * >( this )->section_after_offset()
        );
    }

    const Section * Split::section_after_offset() const noexcept
    {
        auto & secs = sections();
        auto sec = secs.begin();
        while ( sec != secs.end() && sec->to() < _offset )
            ++sec;
        return sec;
    }

    void Split::insert_section_at( Section * at, Section && sec ) noexcept
    {
        auto & secs = sections();
        if ( at == secs.end() ) {
            secs.push_back( std::move( sec ) );
        } else {
            if ( at->to() == _offset )
                at->merge( &sec );
            else
                secs.insert( at, std::move( sec ) );
        }
    }

    void Split::shrink_correction( Section * sec, Segment * bound ) noexcept
    {
        if ( sec->length() == 0 ) {
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

    void Split::divide( Section * sec, Segment & seg, size_t idx ) noexcept
    {
        auto [left, right] = seg.divide( idx );

        Section right_section;
        if ( !right.empty() )
            right_section.append( std::move( right ) );
        for ( auto rest = std::next( &seg ); rest != sec->end(); ++rest )
            right_section.append( std::move( *rest ) );

        sec->erase( &seg, sec->end() );
        if ( !left.empty() )
            sec->append( std::move( left ) );

        sections().insert( std::next( sec ), right_section );
    }

    void Split::write_zero( size_t idx ) noexcept
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

    void Split::overwrite_char( Section * sec, Segment & seg, size_t idx, char val ) noexcept
    {
        if ( seg.size() == 1 ) {
            seg.value = val;
            sec->merge_neighbours( &seg );
        } else {
            auto [left, right] = seg.divide( idx );

            seg.from = idx;
            seg.to = idx + 1;
            seg.value = val;

            auto place = &seg;
            if ( !left.empty() ) {
                auto it = sec->insert( place, left );
                place = std::next( it );
            }

            if ( !right.empty() ) {
                auto it = sec->insert( std::next( place ), right );
                place = std::prev( it );
            }

            if ( left.empty() || right.empty() )
                sec->merge_neighbours( place );
        }
    }

    void Split::overwrite_zero( size_t idx, char val ) noexcept
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

    void Split::write_char( size_t idx, char val ) noexcept
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

    void Split::write( char val ) noexcept
    {
        write( 0, val );
    }

    void Split::write( size_t idx, char val ) noexcept
    {
        idx = idx + _offset;
        assert( idx < size() );
        if ( val == '\0' )
            write_zero( idx );
        else
            write_char( idx, val );
    }


    char Split::read() const noexcept
    {
        return read( 0 );
    }

    char Split::read( size_t idx ) const noexcept
    {
        idx = idx + _offset;
        assert( idx < size() );
        if ( auto sec = section_of( idx ) )
            return sec->segment_of( idx ).value;
        return '\0';
    }

    Split * Split::offset( size_t off ) const noexcept
    {
        auto split = __new< Split >( _VM_PT_Heap, _data );
        split->_offset = _offset + off;
        return split;
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
