#include <rst/formula.h>
#include <dios.h>

#if __cplusplus >= 201103L

namespace abstract::sym {

__invisible void formula_release( Formula * formula ) noexcept {
    formula->refcount_decrement();
}

__invisible void formula_cleanup( Formula * formula ) noexcept {
    if ( isUnary( formula->op() ) ) {
        auto * child = formula->unary.child;
        child->refcount_decrement();
        formula_cleanup_check( child );
    }
    else if ( isBinary( formula->op() ) ) {
        auto * left = formula->binary.left;
        auto * right = formula->binary.right;
        left->refcount_decrement();
        formula_cleanup_check( left );
        right->refcount_decrement();
        formula_cleanup_check( right );
    }

    __dios_safe_free( formula );
}

__invisible void formula_cleanup_check( Formula * formula ) noexcept {
    if ( !formula || formula->refcount() )
        return;
    formula_cleanup( formula );
}

} // namespace abstract::sym

#endif
