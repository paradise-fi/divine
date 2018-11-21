/*
 * (c) 2018 Tadeáš Kučera <kucerat@mail.muni.cz>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include<set>
#include<map>
#include<optional>
#include<dios.h>
#include<sys/monitor.h>

//Degeneralizer with level reset and level caching, based on:
//"Compositional Approach to Suspension and Other Improvements to LTL Translation"
struct Degeneralizer2
{
    size_t current, last;
    std::map< size_t, size_t > visited;

    Degeneralizer2() = delete;
    Degeneralizer2( size_t _numberOfAcceptingSets )
        : current( _numberOfAcceptingSets )
        , last( _numberOfAcceptingSets )
    {
    }
    size_t knownLevelFor( size_t dest ) {
        auto it = visited.find( dest );
        if( it != visited.end() )
            return it->second;
        markVisited( dest, 0 );
        return 0;
    }
    void markVisited( size_t state, size_t level ) {
        visited.emplace( std::make_pair( state, level ) );
    }
    // @acc_sets: ids of accepting sets, that current transition belongs to
    // @scc1,@scc2: optional id of the accepting SCC that the first/second node belongs to
    // @dest: id of the target of fired tgba transition
    bool step( const std::set< size_t >& acc_sets, std::optional< size_t > scc1, std::optional< size_t > scc2, size_t dest ) {
        if( !scc2.has_value() )
            current = 0;
        else if( scc1 != scc2 )
            current = knownLevelFor( dest );
        else if( scc1 == scc2 )
        {
            assert( scc1.has_value() && scc2.has_value() );
            if( current == last )
                current = 0;
            auto it = acc_sets.begin();
            if( current != 0 )
                it = acc_sets.find( current );
            for( ; it != acc_sets.end(); ++it, ++current )
                if( *it != current )
                    break;
        }
        markVisited( dest, current );
        return current == last;
    }
};
template< typename TGBA >
struct Monitor : public __dios::Monitor {
    TGBA tgba;
    Degeneralizer2 deg;

    Monitor( TGBA&& _tgba )
        : tgba( _tgba )
        , deg( _tgba.numberAcc )
    {
    }
    // preformes step on tgba, if succesful then degeneralizer.step() too.
    void step() override {
        std::set< size_t > acc;
        std::optional< size_t > scc1;
        std::optional< size_t > scc2;
        size_t dest;
        if( !tgba.step( acc, scc1, scc2, dest ) ) //step was not succesful
        {
            monitor_cancel();
            return;
        }
        if( deg.step( acc, scc1, scc2, dest ) ) // we got to accepting state
            monitor_accept();
    }
};
