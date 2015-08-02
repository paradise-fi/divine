find_package(Qt4 REQUIRED QtXml)
include(${QT_USE_FILE})

include_directories( ${DIVINE_INCLUDES} ${QT_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR} )
include_directories( SYSTEM ${DIVINE_SYS_INCLUDES} )

set(BASE_IDE_SRC
    modules.h

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
    moduleManager.cpp
    moduleManager.h

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
    divineTools.cpp
    divineTools.h
)

set(BASE_IDE_MOC
    modules.h
    abstractDocument.h
    abstractEditor.h
    abstractSimulator.h
    mainForm.h
    layoutManager.h
    moduleManager.h
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
    divineTools.h
)

set(BASE_IDE_UI
    ui/newDocumentDialog.ui
    ui/multiSaveDialog.ui
    ui/searchPanel.ui
)

qt4_wrap_cpp(BASE_IDE_SRC2 ${BASE_IDE_MOC})
qt4_wrap_ui(BASE_IDE_SRC3 ${BASE_IDE_UI})

add_library(baseIDE STATIC ${BASE_IDE_SRC} ${BASE_IDE_SRC2} ${BASE_IDE_SRC3})
target_link_libraries(baseIDE libdivine ${LIBDIVINE_INSTANCES} ${QT_LIBRARIES})
