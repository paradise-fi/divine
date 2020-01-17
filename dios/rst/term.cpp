// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#include <dios/lava/term.hpp>
#include <util/array.hpp>
#include <util/map.hpp>

#include <cassert>

namespace __lava
{
    term_state_t * __term_state;

    {
    auto update_rpns( smt::token::VarID id )
    {
        auto& dcmp = __term_state->decomp;
        auto it = dcmp.find( id );
        if( it != dcmp.end() )
        {
            auto repr = *__term_state->uf.find( id );
            if( repr != id )
            {
                auto repr_it = dcmp.find( repr );
                if( repr_it != dcmp.end() )
                {
                    repr_it->second.extend( it->second );
                    repr_it->second.apply< smt::Op::Constraint >();
                    __vm_obj_free( it->second.pointer );
                    __vm_trace( _VM_T_Assume, repr_it->second.pointer );
                }
                else
                {
                    dcmp.emplace( repr, it->second );
                    __vm_trace( _VM_T_Assume, ( dcmp.find( repr )->second ).pointer );
                }
                dcmp.erase( id );
            }
        }
    }

    template< typename T > using stack_t = Array< T >;

    /* Add a constraint to the term. A constraint is again a term_t, e.g. a > 7.
     * !`expect` is for when an else branch was taken, in which case the tested
     * condition had to be false. */
    term_t term_t::constrain( term_t &constraint, bool expect ) const noexcept
    {
        if ( !expect )
        {
            constraint = constraint.make_term( constraint );
            constraint.apply< Op::Not >();
        }

        auto append_term = [&]( term_state_t::var_id_t var )
        {
            auto it = __term_state->decomp.find( var );
            if ( it == __term_state->decomp.end() )
                __term_state->decomp.emplace( var, constraint );
            else
            {
                it->second.extend( constraint );
                __vm_obj_free( constraint.pointer );
                it->second.apply< Op::And >();
            }
        };

        auto id = brick::smt::decompose< stack_t >( constraint.as_rpn(), __term_state->uf, update_rpns );
        if ( !id )
            __vm_trace( _VM_T_Assume, constraint.pointer );
        else  // append to relevant decomp
        {
            append_term( *id );
            __vm_trace( _VM_T_Assume, ((*__term_state->decomp.find( *id )).second).pointer );
        }

        return *this;
    }

}

void *__dios_term_init()
{
    using namespace __dios::rst::abstract;
    auto true_term = term_t::lift_one_i1( true );
    __term_state = static_cast< term_state_t * >( __vm_obj_make( sizeof( term_state_t ), _VM_PT_Heap ) );
    new ( __term_state ) term_state_t;
    __term_state->uf.make_set( 0 );
    __term_state->decomp[ 0 ] = true_term;
    return __term_state->decomp._container._data;
}

extern "C" __invisible void __dios_term_fini()
{
    using namespace __dios::rst::abstract;
    for ( auto [ key, term ] : __term_state->decomp )
        __vm_obj_free( term.pointer );
    __term_state->decomp.clear();
    __term_state->uf.clear();
    __vm_obj_free( __term_state );
}
