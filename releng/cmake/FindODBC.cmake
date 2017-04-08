find_program( ODBC_CONFIG_EXECUTABLE
              NAMES odbc_config iodbc-config
              DOC "path to the odbc_config executable" )

get_filename_component( ODBC_CONFIG_REAL ${ODBC_CONFIG_EXECUTABLE} REALPATH )
get_filename_component( ODBC_CONFIG_NAME ${ODBC_CONFIG_EXECUTABLE} NAME_WE )

if( ODBC_CONFIG_EXECUTABLE )

  if ( ${ODBC_CONFIG_NAME} EQUAL "iodbc-config" )
      exec_program(${ODBC_CONFIG_REAL} ARGS --lib-prefix     OUTPUT_VARIABLE ODBC_LIBRARY_DIRS_ )
      exec_program(${ODBC_CONFIG_REAL} ARGS --include-prefix OUTPUT_VARIABLE ODBC_INCLUDE_DIRS_ )
  else()
      exec_program(${ODBC_CONFIG_REAL} ARGS --prefix     OUTPUT_VARIABLE ODBC_PREFIX_DIR )
      set( ODBC_LIBRARY_DIRS_ "${ODBC_PREFIX_DIR}/lib" )
      set( ODBC_INCLUDE_DIRS_ "${ODBC_PREFIX_DIR}/include" )
  endif()
  exec_program(${ODBC_CONFIG_REAL} ARGS --cflags         OUTPUT_VARIABLE ODBC_COMPILE_FLAGS_ )
  exec_program(${ODBC_CONFIG_REAL} ARGS --libs           OUTPUT_VARIABLE ODBC_LIBRARIES )
  string(REGEX REPLACE "-DNDEBUG" "" ODBC_COMPILE_FLAGS ${ODBC_COMPILE_FLAGS_})
  string(REGEX REPLACE " +" ";" ODBC_SYSLIBS1 ${ODBC_LIBRARIES})
  foreach( lib ${ODBC_SYSLIBS1} )
      if ( lib MATCHES "^-l" )
          string(REPLACE "-l" "" lib1 ${lib})
          list( APPEND ODBC_SYSLIBS ${lib1} )
      endif()
  endforeach()
endif()

set(ODBC_INCLUDE_DIRS ${ODBC_INCLUDE_DIRS_} CACHE STRING "ODBC include path")
set(ODBC_LIBRARY_DIRS ${ODBC_LIBRARY_DIRS_} CACHE STRING "ODBC library path")

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(ODBC DEFAULT_MSG ODBC_CONFIG_EXECUTABLE ODBC_INCLUDE_DIRS_)
