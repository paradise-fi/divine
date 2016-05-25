#ifndef __DIVINE_METADATA_H__
#define __DIVINE_METADATA_H__

#ifdef __cplusplus
#define EXTERN_C extern "C" {
#define CPP_END }
#define NOTHROW noexcept
#else
#define EXTERN_C
#define CPP_END
#define NOTHROW
#endif

EXTERN_C

typedef struct {
    int opcode;
    int subop;
} _MD_InstInfo;

typedef struct {
    char *name;
    void (*entry_point)();
    int frame_size;
    int arg_count;
    int is_variadic;
    int inst_table_size;
    _MD_InstInfo *inst_table;
} _MD_Function;

const _MD_Function *__md_get_function_meta( const char *name );

CPP_END

#endif // __DIVINE_METADATA_H__
