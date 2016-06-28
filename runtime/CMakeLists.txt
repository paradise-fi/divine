add_subdirectory( native )

include( stringify )

file( GLOB SRC_LLVM_USR_CPP RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp divine/*.cpp )
file( GLOB SRC_LLVM_USR_H RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.h divine/*.h atomic sys/*.h bits/*.h )
foreach( f ${SRC_LLVM_USR_H} )
    stringify( "llvm" ${f} )
endforeach()
stringlist( "llvm" llvm_h )
set( STR_LLVM_USR_H ${STRINGIFIED} )
unset( STRINGIFIED )
unset( STRINGIFIED_FILES )

foreach( f ${SRC_LLVM_USR_CPP} )
    stringify( "llvm" ${f} )
endforeach()
stringlist( "llvm" llvm )

add_library( llvm_usr_str STATIC ${STR_LLVM_USR_H} ${STRINGIFIED} llvm_h_list.cpp llvm_list.cpp )
