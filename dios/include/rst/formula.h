#pragma once
#include <sys/lart.h>
#include <rst/common.h>

#if __cplusplus >= 201103L

namespace abstract::sym {
    using Formula = lart::sym::Formula;

    void formula_release( Formula * formula ) noexcept;

    void formula_cleanup( Formula * formula ) noexcept;
    void formula_cleanup_check( Formula * formula ) noexcept;

    using Release = decltype( formula_release );
    using Check = decltype( formula_cleanup_check );

} // namespace lart::sym

namespace abstract {
    using Object = lart::sym::Formula;
    using Release = abstract::sym::Release;
    using Check = abstract::sym::Check;

    // explicitly instantiate because of __invisible attribute
    template __invisible void cleanup< Object, Release, Check >(
        _VM_Frame * frame, Release release, Check check
    ) noexcept;
} // namespace abstract

namespace abstract::sym {
    __invisible static inline void cleanup( _VM_Frame * frame ) noexcept {
        abstract::cleanup< Formula, Release, Check >( frame, formula_release, formula_cleanup_check );
    }
} // namespace lart::sym

#endif
