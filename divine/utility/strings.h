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
}

struct stringtable { const char *n, *c; };
extern stringtable llvm_list[];
extern stringtable llvm_h_list[];
}

#endif
