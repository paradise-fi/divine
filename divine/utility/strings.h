#ifndef DIVINE_STRINGS_H
#define DIVINE_STRINGS_H

namespace divine {
extern const char *cesmi_usr_cesmi_h_str;
extern const char *cesmi_usr_cesmi_cpp_str;
extern const char *cesmi_usr_ltl_cpp_str;
extern const char *cesmi_usr_ltl_h_str;
extern const char *toolkit_pool_h_str;
extern const char *toolkit_blob_h_str;
extern const char *compile_defines_str;

namespace src_llvm {
    extern const char *llvm_problem_def_str;
    extern const char *divine_h_str;
    extern const char *pthread_h_str;
    extern const char *unwind_h_str;
    extern const char *divine_assert_h_str;
    extern const char *divine_problem_h_str;
    extern const char *divine_atomoic_str;

    extern const char *cxa_exception_divine_cpp_str;
    extern const char *glue_cpp_str;
    extern const char *pthread_cpp_str;
    extern const char *entry_cpp_str;
    extern const char *stubs_cpp_str;
}

struct stringtable { const char *n, *c; };
extern stringtable llvm_list[];
extern stringtable llvm_h_list[];
}

#endif
