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
                new_it->second.apply( old_it->second, brq::smt_op::constraint );
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
    term term::assume( term t, term c, bool expect )
    {
        if ( !expect )
            c = term( c, op::bv_not );
        auto &decomp = __term_state->decomp;

        auto append_term = [&]( brq::smt_varid_t var )
        {
            if ( auto it = decomp.find( var ); it != decomp.end() )
                it->second.apply( c, brq::smt_op::bool_and );
            else
                decomp.emplace( var, c );
        };

        auto id = brq::smt_decompose< stack_t >( c, __term_state->uf, update_rpns );

        if ( !id )
            __vm_trace( _VM_T_Assume, c.begin() );
        else  // append to relevant decomp
        {
            append_term( id );
            __vm_trace( _VM_T_Assume, decomp.find( id )->second.begin() );
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
