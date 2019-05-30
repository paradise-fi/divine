// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#include <rst/term.hpp>

#include <cassert>

namespace __dios::rst::abstract {
    TermState __term_state;

    Tristate Term::to_tristate() noexcept
    {
        return { Tristate::Value::Unknown };
    }

    Term Term::constrain( Term constraint, bool expect ) const noexcept
    {
        if ( !expect )
            constraint.apply_in_place_op< Op::Not, 1 >();
        __term_state.constraints.apply_in_place< Op::Constraint, 1 >( constraint );
        __vm_trace( _VM_T_Assume, __term_state.constraints.pointer() );
        return *this;
    }
} // namespace abstract
