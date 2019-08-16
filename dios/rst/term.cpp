// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#include <rst/term.hpp>

#include <cassert>

namespace __dios::rst::abstract {

    TermState __term_state;

    template< typename C >
    _LART_INLINE C make_term() noexcept
    {
        return make_abstract< C, Term >();
    }
    template< typename C >
    _LART_INLINE C make_term( C c ) noexcept
    {
        return make_abstract< C, Term >( c );
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

    /* Add a constraint to the term. A constraint is again a Term, e.g. a > 7.
     * !`expect` is for when an else branch was taken, in which case the tested
     * condition had to be false. */
    Term Term::constrain( const Term &constraint, bool expect ) const noexcept
    {
        auto & pc = __term_state.constraints;

        if ( !__term_state.constraints.pointer )
            pc = Term::lift_one_i1( true );

        pc.extend( constraint );
        if ( !expect )
            pc.apply< Op::Not >();
        pc.apply< Op::Constraint >();

        __vm_trace( _VM_T_Assume, pc.pointer );
        return *this;
    }

} // namespace __dios::rst::abstract
