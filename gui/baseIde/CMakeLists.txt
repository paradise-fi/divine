find_package(Qt4 REQUIRED QtXml)
include(${QT_USE_FILE})

include_directories(${QT_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR})

set(BASE_IDE_SRC
    baseIdeExport.h
    plugins.h

    # abstract
    abstractDocument.h
    abstractEditor.h
    abstractSimulator.h
    abstractToolLock.h

    # base
    mainForm.cpp
    mainForm.h
    layoutManager.cpp
    layoutManager.h
    pluginManager.cpp
    pluginManager.h

    recentFilesMenu.cpp
    recentFilesMenu.h
    settings.cpp
    settings.h
    simulationProxy.cpp
    simulationProxy.h
    simulationTools.cpp
    simulationTools.h

    # base implementation
    textDocument.cpp
    textDocument.h
    textEditor.cpp
    textEditor.h
    textEditorHandlers.cpp
    textEditorHandlers.h

    # embedded widgets
    lineNumberBar.cpp
    lineNumberBar.h
    searchPanel.cpp
    searchPanel.h

    # dialogs
    multiSaveDialog.cpp
    multiSaveDialog.h
    newDocumentDialog.cpp
    newDocumentDialog.h
    preferencesDialog.cpp
    preferencesDialog.h

    # utility
    signalLock.h
    prettyPrint.cpp
    prettyPrint.h
)

set(BASE_IDE_MOC
    plugins.h
    abstractDocument.h
    abstractEditor.h
    abstractSimulator.h
    mainForm.h
    layoutManager.h
    pluginManager.h
    recentFilesMenu.h
    simulationProxy.h
    simulationTools.h
    textDocument.h
    textEditor.h
    textEditorHandlers.h
    lineNumberBar.h
    searchPanel.h
    multiSaveDialog.h
    newDocumentDialog.h
    preferencesDialog.h
)

set(BASE_IDE_UI
    ui/newDocumentDialog.ui
    ui/multiSaveDialog.ui
    ui/searchPanel.ui
)

set(BASE_IDE_RES
    baseIde.qrc
)

add_definitions(-DBASE_IDE_LIB)

qt4_wrap_cpp(BASE_IDE_SRC2 ${BASE_IDE_MOC})
qt4_wrap_ui(BASE_IDE_SRC3 ${BASE_IDE_UI})
qt4_add_resources(BASE_IDE_SRC4 ${BASE_IDE_RES})

add_library(baseIDE SHARED ${BASE_IDE_SRC} ${BASE_IDE_SRC2}
                           ${BASE_IDE_SRC3} ${BASE_IDE_SRC4})
target_link_libraries(baseIDE ${QT_LIBRARIES})

install(TARGETS baseIDE DESTINATION ${GUI_LIB_DEST} COMPONENT gui_tools)
