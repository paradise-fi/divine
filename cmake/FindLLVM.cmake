find_program(LLVM_CONFIG_EXECUTABLE
             NAMES llvm-config llvm-config-3.4 llvm-config-3.4.1 llvm-config-3.4.2 llvm-config-3.3 llvm-config-mp-3.4 llvm-config-mp-3.3
             DOC "path to the llvm-config executable")

# llvm-config does not like to be called through a symlink
get_filename_component(LLVM_CONFIG_REAL ${LLVM_CONFIG_EXECUTABLE} REALPATH)

if(LLVM_CONFIG_EXECUTABLE)
  exec_program(${LLVM_CONFIG_REAL} ARGS --version OUTPUT_VARIABLE LLVM_STRING_VERSION )
  exec_program(${LLVM_CONFIG_REAL} ARGS --libdir OUTPUT_VARIABLE LLVM_LIBRARY_DIRS_ )
  exec_program(${LLVM_CONFIG_REAL} ARGS --includedir OUTPUT_VARIABLE LLVM_INCLUDE_DIRS_ )
  exec_program(${LLVM_CONFIG_REAL} ARGS --cppflags OUTPUT_VARIABLE LLVM_COMPILE_FLAGS_ )
  exec_program(${LLVM_CONFIG_REAL} ARGS --ldflags   OUTPUT_VARIABLE LLVM_LDFLAGS )
  exec_program(${LLVM_CONFIG_REAL} ARGS --libs OUTPUT_VARIABLE LLVM_LIBRARIES )
  exec_program(${LLVM_CONFIG_REAL} ARGS --system-libs OUTPUT_VARIABLE LLVM_SYSLIBS )
endif()

set(LLVM_INCLUDE_DIRS ${LLVM_INCLUDE_DIRS_} CACHE STRING "LLVM include path")
set(LLVM_LIBRARY_DIRS ${LLVM_LIBRARY_DIRS_} CACHE STRING "LLVM library path")

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LLVM DEFAULT_MSG LLVM_CONFIG_EXECUTABLE LLVM_INCLUDE_DIRS_)
