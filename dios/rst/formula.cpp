#include <rst/formula.h>
#include <dios.h>

#if __cplusplus >= 201103L

namespace abstract::sym {

__invisible void formula_release( Formula * formula ) noexcept {
    formula->refcount_decrement();
}

__invisible void formula_cleanup( Formula * formula ) noexcept {
    if ( isUnary( formula->op() ) ) {
        if ( auto * child = formula->unary.child ) {
            child->refcount_decrement();
            formula_cleanup_check( child );
        }
    }
    else if ( isBinary( formula->op() ) ) {
        if ( auto * left = formula->binary.left ) {
            left->refcount_decrement();
            formula_cleanup_check( left );
        }

        if ( auto * right = formula->binary.right ) {
            right->refcount_decrement();
            formula_cleanup_check( right );
        }
    }

    __dios_safe_free( formula );
}

__invisible void formula_cleanup_check( Formula * formula ) noexcept {
    cleanup_check( formula, formula_cleanup );
}

} // namespace abstract::sym

#endif
