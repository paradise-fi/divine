// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#include <dios/lava/term.hpp>
#include <util/array.hpp>
#include <util/map.hpp>

#include <cassert>

namespace __lava
{
    term_state_t * __term_state;

    void update_rpns( brq::smt_varid_t old, brq::smt_varid_t new_ )
    {
        auto &decomp = __term_state->decomp;

        if ( old == new_ )
            return;

        if ( auto old_it = decomp.find( old ); old_it != decomp.end() )
        {
            if ( auto new_it = decomp.find( new_ ); new_it != decomp.end() )
            {
                new_it->second.apply( old_it->second, brq::smt_op::bool_and );
                __vm_trace( _VM_T_Assume, new_it->second.begin() );
            }
            else
            {
                decomp.emplace( new_, old_it->second );
                __vm_trace( _VM_T_Assume, decomp.find( new_ )->second.begin() );
            }

            decomp.erase( old_it );
        }
    }

    template< typename T > using stack_t = __dios::Array< T >;

    /* Add a constraint to the term. A constraint is again a term_t, e.g. a > 7.
     * !`expect` is for when an else branch was taken, in which case the tested
     * condition had to be false. */
    __flatten term term::assume( const term &t, const term &c, bool expect )
    {
        auto &decomp = __term_state->decomp;
        auto &uf = __term_state->uf;

        int id = 0;
        auto it = decomp.begin();
        auto at = c.begin();

        for ( ; at != c.end(); ++at )
            if ( auto x = at->varid() )
            {
                id = uf.make_set( x );
                if ( auto ex = decomp.find( id ); ex != decomp.end() )
                {
                    it = ex;
                    if ( expect )
                        ex->second.apply( c, brq::smt_op::bool_and );
                    else
                        ex->second.apply( c, brq::smt_op::bool_not, brq::smt_op::bool_and );
                }
                else
                {
                    it = decomp.emplace( id, c ).first;
                    if ( !expect )
                        it->second.apply( brq::smt_op::bool_not );
                }
                break;
            }

        for ( ; at != c.end(); ++at )
            if ( auto x = at->varid() )
            {
                int x_id = uf.make_set( x );
                if ( id == x_id )
                    continue;
                uf.join( id, x_id );
                if ( auto ex = decomp.find( x_id ); ex != decomp.end() )
                {
                    it->second.apply( ex->second, brq::smt_op::bool_and );
                    decomp.erase( ex ); /* might invalidate 'it' */
                    it = decomp.find( id );
                }
            }

        if ( id )
            __vm_trace( _VM_T_Assume, it->second.unsafe_ptr() );
        else
        {
            auto trace = expect ? term( c ) : term( c, brq::smt_op::bool_not );
            __vm_trace( _VM_T_Assume, trace.disown() ); /* FIXME leak */
        }

        return t;
    }

}

void *__dios_term_init()
{
    using namespace __lava;
    auto true_term = term::lift_i1( true );
    __term_state = static_cast< term_state_t * >( __vm_obj_make( sizeof( term_state_t ), _VM_PT_Heap ) );
    new ( __term_state ) term_state_t;
    __term_state->uf.make_set( 0 );
    __term_state->decomp[ 0 ] = true_term;
    return __term_state->decomp._container._data;
    return nullptr;
}

extern "C" __invisible void __dios_term_fini()
{
    using namespace __lava;
    __term_state->decomp.clear();
    __term_state->uf.clear();
    __vm_obj_free( __term_state );
}
