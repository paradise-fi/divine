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

    extern "C" {
        _LART_SCALAR uint8_t __term_val_i8() { return make_term< uint8_t >(); }
        _LART_SCALAR uint16_t __term_val_i16() { return make_term< uint16_t >(); }
        _LART_SCALAR uint32_t __term_val_i32() { return make_term< uint32_t >(); }
        _LART_SCALAR uint64_t __term_val_i64() { return make_term< uint64_t >(); }
    }

    Term Term::constrain( Term constraint, bool expect ) const noexcept
    {
        if ( !__term_state.constraints.pointer )
            __term_state.constraints = Term::lift_one_i1( true );
        if ( !expect )
            constraint.apply_in_place_op< Op::Not >();
        __term_state.constraints.apply_in_place< Op::Constraint >( constraint );
        __vm_trace( _VM_T_Assume, __term_state.constraints.pointer );
        return *this;
    }

} // namespace __dios::rst::abstract
