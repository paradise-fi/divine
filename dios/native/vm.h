#ifndef __cplusplus
#error DIVINE native interface can be only accessed from C++
#endif

#ifndef __RUNTIME_NATIVE_VM_H__
#define __RUNTIME_NATIVE_VM_H__

#include <libunwind.h>
#include <brick-types>
#include <pthread.h>

namespace nativeRuntime {

struct FrameEx {
    _VM_Frame *child;
    long tid;
    unw_context_t *uwctx;
    unw_cursor_t uwcur;
};

}

#endif
