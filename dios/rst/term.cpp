// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#include <rst/term.hpp>
#include <util/array.hpp>
#include <util/map.hpp>

#include <cassert>

namespace __dios::rst::abstract
{
    term_state_t * __term_state;

    template< typename concrete_t >
    _LART_INLINE concrete_t make_term() noexcept
    {
        return make_abstract< concrete_t, term_t >();
    }

    template< typename concrete_t >
    _LART_INLINE concrete_t make_term( concrete_t c ) noexcept
    {
        return make_abstract< concrete_t, term_t >( c );
    }

    extern "C" {
        _LART_SCALAR uint8_t __sym_val_i8() { return make_term< uint8_t >(); }
        _LART_SCALAR uint16_t __sym_val_i16() { return make_term< uint16_t >(); }
        _LART_SCALAR uint32_t __sym_val_i32() { return make_term< uint32_t >(); }
        _LART_SCALAR uint64_t __sym_val_i64() { return make_term< uint64_t >(); }
        _LART_SCALAR float __sym_val_float32() { return make_term< float >(); }
        _LART_SCALAR double __sym_val_float64() { return make_term< double >(); }

        _LART_SCALAR uint8_t __sym_lift_i8( uint8_t c ) { return make_term< uint8_t >( c ); }
        _LART_SCALAR uint16_t __sym_lift_i16( uint16_t c ) { return make_term< uint16_t >( c ); }
        _LART_SCALAR uint32_t __sym_lift_i32( uint32_t c ) { return make_term< uint32_t >( c ); }
        _LART_SCALAR uint64_t __sym_lift_i64( uint64_t c ) { return make_term< uint64_t >( c ); }
        _LART_SCALAR float __sym_lift_float32( float c ) { return make_term< float >( c ); }
        _LART_SCALAR double __sym_lift_float64( double c ) { return make_term< double >( c ); }
    }

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
            constraint.apply< Op::Not >();

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
