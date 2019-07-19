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
        _LART_SCALAR uint8_t __sym_val_i8() { return make_term< uint8_t >(); }
        _LART_SCALAR uint16_t __sym_val_i16() { return make_term< uint16_t >(); }
        _LART_SCALAR uint32_t __sym_val_i32() { return make_term< uint32_t >(); }
        _LART_SCALAR uint64_t __sym_val_i64() { return make_term< uint64_t >(); }
    }

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
