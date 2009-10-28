project(gui)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

include_directories(include)
add_subdirectory(main)
add_subdirectory(base_shared)
add_subdirectory(base_tools)
add_subdirectory(dve_editor)
add_subdirectory(dve_divine2)
add_subdirectory(ltl_editor)
