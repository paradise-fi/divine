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
    return __divine_malloc( count );
#else
    return std::malloc( count );
#endif
}

void *operator new[]( std::size_t count, const divine::fs::memory::nofail_t &nofail ) noexcept {
    return ::operator new( count, nofail );
}
