find_package(Qt4 REQUIRED)
include(${QT_USE_FILE})

include_directories(${QT_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR} )

set(HDRS ../include)

set(BASE_SHARED_SRC
    # shared headers
    ${HDRS}/mainform.h
    ${HDRS}/settings.h
    ${HDRS}/plugins.h
    ${HDRS}/layout.h
    ${HDRS}/editor.h
    ${HDRS}/simulator.h
    ${HDRS}/print.h
    
    mainform.cpp
    settings.cpp
    layout.cpp
    editor.cpp
    print.cpp
    simulator.cpp
    recent.cpp
    recent.h
    
    # dialogs
    multisave_dlg.cpp
    multisave_dlg.h
    preferences_dlg.cpp
    preferences_dlg.h
    search_dlg.cpp
    search_dlg.h
)

set(BASE_SHARED_MOC
    ${HDRS}/mainform.h
    ${HDRS}/plugins.h
    ${HDRS}/layout.h
    ${HDRS}/editor.h
    ${HDRS}/simulator.h
    
    recent.h
    multisave_dlg.h
    preferences_dlg.h
    search_dlg.h
)

set(BASE_SHARED_UI
    ui/multisave.ui
    ui/search.ui
)

set(BASE_SHARED_RES
    mainform.qrc
)

add_definitions(-DBASE_SHARED_LIB)

qt4_wrap_cpp(BASE_SHARED_SRC2 ${BASE_SHARED_MOC})
qt4_wrap_ui(BASE_SHARED_SRC3 ${BASE_SHARED_UI})
qt4_add_resources(BASE_SHARED_SRC4 ${BASE_SHARED_RES})

add_library(base_shared SHARED ${BASE_SHARED_SRC} ${BASE_SHARED_SRC2}
  ${BASE_SHARED_SRC3} ${BASE_SHARED_SRC4})
target_link_libraries(base_shared ${QT_LIBRARIES})

install(TARGETS base_shared DESTINATION ${GUI_LIB_DEST} COMPONENT gui_tools)
