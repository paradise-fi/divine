find_package(Qt4 REQUIRED)
include(${QT_USE_FILE})

include_directories(${QT_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR})

set(PICKER_SRC
    qtcolorpicker.cpp
    qtcolorpicker.h
)

qt4_generate_moc(qtcolorpicker.cpp ${CMAKE_CURRENT_BINARY_DIR}/qtcolorpicker.moc)
qt4_generate_moc(qtcolorpicker.h ${CMAKE_CURRENT_BINARY_DIR}/moc_qtcolorpicker.cpp)

add_library(qtcolorpicker STATIC ${PICKER_SRC}
qtcolorpicker.moc
moc_qtcolorpicker.cpp)

if(NOT WIN32)
  set_target_properties(qtcolorpicker PROPERTIES COMPILE_FLAGS -fPIC)
endif(NOT WIN32)

target_link_libraries(qtcolorpicker ${QT_LIBRARIES})
