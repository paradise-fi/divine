#include <unwind.h>

__attribute__((noreturn)) void klee_abort();

void _Unwind_SetGR( _Unwind_Context *, int, uintptr_t ) {}
void _Unwind_SetIP( _Unwind_Context *, uintptr_t ) {}
uintptr_t _Unwind_GetGR( _Unwind_Context *, int ) { return 0; }
uintptr_t _Unwind_GetIP( _Unwind_Context *) { return 0; }

_Unwind_Reason_Code _Unwind_RaiseException( _Unwind_Exception * )
{
    klee_abort();
}

void _Unwind_Resume( _Unwind_Exception * ) { klee_abort(); }
void _Unwind_DeleteException( _Unwind_Exception * ) {}

uintptr_t _Unwind_GetLanguageSpecificData( _Unwind_Context * ) { return 0; }
uintptr_t _Unwind_GetRegionStart( _Unwind_Context * ) { return 0; }
