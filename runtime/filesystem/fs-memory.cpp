// -*- C++ -*- (c) 2015 Jiří Weiser

#include "fs-memory.h"

namespace divine {
namespace fs {
namespace memory {
    nofail_t nofail;
} // namespace memory
} // namespace fs
} // namespace divine

void *operator new( std::size_t count, const divine::fs::memory::nofail_t & ) noexcept {
#ifdef __divine__
    return __vm_obj_make( count );
#else
    return ::operator new( count );
#endif
}

void *operator new[]( std::size_t count, const divine::fs::memory::nofail_t &nofail ) noexcept {
#ifdef __divine__
    return ::operator new( count, nofail );
#else
    return ::operator new[]( count );
#endif
}
