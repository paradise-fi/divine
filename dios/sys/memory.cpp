// -*- C++ -*- (c) 2015 Jiří Weiser
//             (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#include <dios/sys/memory.hpp>

namespace __dios {
    nofail_t nofail;
} // namespace __dios

void *operator new( std::size_t count, const __dios::nofail_t & ) noexcept {
#ifdef __divine__
    return __vm_obj_make( count, _VM_PT_Heap );
#else
    return ::operator new( count );
#endif
}

void *operator new[]( std::size_t count, const __dios::nofail_t &nofail ) noexcept {
#ifdef __divine__
    return ::operator new( count, nofail );
#else
    return ::operator new[]( count );
#endif
}
