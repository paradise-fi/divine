#pragma once
#include <sys/lart.h>

#if __cplusplus >= 201103L

#include <util/array.hpp>

namespace lart::sym {

__dios::Array< Formula * > __orphan_formulae( _VM_Frame * frame ) noexcept;

void __formula_cleanup( Formula * formula ) noexcept;

void __cleanup_orphan_formulae( _VM_Frame * frame ) noexcept;

} // namespace lart::sym

#endif
