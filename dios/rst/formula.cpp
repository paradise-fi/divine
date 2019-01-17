#include <rst/formula.h>
#include <dios.h>

#if __cplusplus >= 201103L

namespace lart::sym {

__invisible void formula_release( Formula * formula ) noexcept {
    formula->refcount_decrement();
}

__invisible void __formula_cleanup( Formula * formula ) noexcept {
    auto clean_child = [] (auto * child) {
        child->refcount_decrement();
        if ( child->refcount() == 0 )
            __formula_cleanup( child );
    };

    if ( isUnary( formula->op() ) ) {
        clean_child( formula->unary.child );
    }
    else if ( isBinary( formula->op() ) ) {
        clean_child( formula->binary.left );
        clean_child( formula->binary.right );
    }

    __dios_safe_free( formula );
}

__invisible void formula_cleanup_check( Formula * formula ) noexcept {
    if ( !formula || formula->refcount() )
        return;
    formula_cleanup( formula );
}

} // namespace lart::sym

#endif
