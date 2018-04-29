#pragma once

#include <cstdint>

extern "C" {
    void __lart_stash( uintptr_t );
    uintptr_t __lart_unstash();
}
