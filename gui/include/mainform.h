/***************************************************************************
 *   Copyright (C) 2009 by Martin Moracek                                  *
 *   xmoracek@fi.muni.cz                                                   *
 *                                                                         *
 *   DiVinE is free software, distributed under GNU GPL and BSD licences.  *
 *   Detailed licence texts may be found in the COPYING file in the        *
 *   distribution tarball. The tool is a product of the ParaDiSe           *
 *   Laboratory, Faculty of Informatics of Masaryk University.             *
 *                                                                         *
 *   This distribution includes freely redistributable third-party code.   *
 *   Please refer to AUTHORS and COPYING included in the distribution for  *
 *   copyright and licensing details.                                      *
 ***************************************************************************/

#ifndef MAINFORM_H_
#define MAINFORM_H_

#include "base_shared_export.h"

#include <QMainWindow>
#include <QMultiHash>
#include <QList>
#include <QPair>
#include <QHash>

class QAction;
class QTabWidget;
class QCloseEvent;
class QProcess;
class QLabel;
class QFileSystemWatcher;
class QTimer;

class RecentFilesMenu;
class PreferencesPage;
class PreferencesDialog;
class SearchDialog;
class LayoutManager;
class SimulatorProxy;
class Simulator;
class SourceEditor;
class EditorBuilder;
class SimulatorFactory;

/*!
 * The MainForm implements the main window of the application.
 */
class BASE_SHARED_EXPORT MainForm : public QMainWindow
{
    Q_OBJECT

  public:
    MainForm(QWidget * parent = 0, Qt::WindowFlags flags = 0);
    ~MainForm();

    // call after plugins are loaded
    void initialize(void);

    // API for plugins
    void registerDocument(const QString & suffix,
                          const QString & filter, EditorBuilder * builder);
    void registerSimulator(const QString & suffix, SimulatorFactory * loader);
    void registerPreferences(const QString & group,
                             const QString & tab, QWidget * page);

    //! Provides access to the layout manager
    LayoutManager * layouts() {return layman_;}
    
    //! Provides access to the current simulator
    SimulatorProxy * simulator() {return simProxy_;}

    // editor access
    int editorCount(void);
    SourceEditor * activeEditor(void);
    SourceEditor * editor(const QString & path);
    SourceEditor * editor(int index);
    const QStringList openedFiles(void) const;
    
    // auto-saving
    bool maybeStop(void);
    bool maybeSave(SourceEditor * editor);
    bool maybeSave(const QList<SourceEditor*> & editors);
    bool maybeSaveAll(void);
    
  public slots:
    void newFile(const QString & hint = QString(), const QByteArray & text = QByteArray());
    void openFile(const QString & path);

    void startSimulation(void);
    
    void updateWindowTitle();
    void updateWorkspace();
    void updateSimulator();
    void updateStatusLabel();
    
    void onDocumentModified(SourceEditor * editor, bool state);

  signals:
    //! Notifies plugins that application settings have been changed.
    void settingsChanged();
    
    /*!
     * User has either switched to another tab or closed the current one.
     * \param editor New active editor.
     */
    void editorChanged(SourceEditor * editor);
    
    /*!
     * Instance of the Simulator class has been changed (created, destroyed).
     * \param simulator The new simulator (or NULL).
     */
    void simulatorChanged(SimulatorProxy * simulator);
    
    //! Forwards message signals from other components.
    void message(const QString & msg);
    
    //! Forwards request for highlighting specified transition.
    void highlightTransition(int id);

  protected:
    void closeEvent(QCloseEvent * event);
    void dragEnterEvent(QDragEnterEvent * event);
    void dropEvent(QDropEvent * event);

  private:
    RecentFilesMenu * recentMenu_;

    QAction * newAct_;
    QAction * openAct_;
    QAction * saveAct_;
    QAction * saveAsAct_;
    QAction * saveAllAct_;
    QAction * reloadAct_;
    QAction * reloadAllAct_;
    QAction * printAct_;
    QAction * previewAct_;
    QAction * closeAct_;
    QAction * closeAllAct_;
    QAction * exitAct_;
    QAction * undoAct_;
    QAction * redoAct_;
    QAction * cutAct_;
    QAction * copyAct_;
    QAction * pasteAct_;
    QAction * overwriteAct_;
    QAction * findAct_;
    QAction * findNextAct_;
    QAction * findPrevAct_;
    QAction * replaceAct_;
    QAction * wordWrapAct_;
    QAction * numbersAct_;
    QAction * prefsAct_;
    QAction * startAct_;
    QAction * stopAct_;
    QAction * restartAct_;
    QAction * stepBackAct_;
    QAction * stepForeAct_;
    QAction * stepRandomAct_;
    QAction * randomRunAct_;
    QAction * importAct_;
    QAction * exportAct_;
    QAction * syntaxAct_;
    QAction * helpAct_;
    QAction * aboutAct_;
    QAction * aboutQtAct_;
    
    QTabWidget * pageArea_;
    QLabel * statusLabel_;       //!< label showing cursor position (row / col)

    // current simulator
    SimulatorProxy * simProxy_;
    
    // layouts
    LayoutManager * layman_;

    // document type info
    typedef QPair<QString, QString> DocTypeInfo; // suffix, filter
    typedef QPair<DocTypeInfo, EditorBuilder*> BuilderPair;
    typedef QList<BuilderPair> BuilderList;
    typedef QHash<QString, SimulatorFactory*> SimulatorHash;

    BuilderList docBuilders_;
    SimulatorHash simLoaders_;

    // persistent dialogs
    PreferencesDialog * preferences_;
    SearchDialog * search_;

    // misc persistent objects
    QProcess * helpProc_;
    QFileSystemWatcher * watcher_;
    QTimer * randomTimer_;
    int steps_;
    
  private:
    void createActions();
    void createMenus();
    void createToolBars();
    void createStatusBar();
    void readSettings();
    void writeSettings();

    EditorBuilder * getBuilder(const QString & suffix);
    
    bool checkSyntaxErrors(Simulator * sim);
    
  private slots:
    void open();
    void openRecent(QAction * action);
    bool save(SourceEditor * editor = NULL);
    bool saveAs(SourceEditor * editor = NULL);
    void saveAll();
    void reload(SourceEditor * editor = NULL);
    void reloadAll();
    void print();
    void preview();
    bool closeFile(SourceEditor * editor = NULL);
    void closeAll();
    void showFindDialog();
    void showReplaceDialog();
    void showPreferences();
    void help(void);
    void about();

    void randomStep(void);
    void randomRun(void);
    void randomTimeout(void);
    void importRun(void);
    void exportRun(void);

    void checkSyntax();
    
    void findNext(void);
    void findPrevious(void);
    void replace(void);
    void replaceAll(void);
    
    void onPrintRequested(QPrinter * printer);
    void onSimulatorStarted(void);
    void onSimulatorStopped(void);
    void onOverwriteToggled(bool checked);
    void onWordWrapToggled(bool checked);
    void onLineNumbersToggled(bool checked);
    void onTabCurrentChanged(int index);
    void onTabCloseRequested(int index);
    void onFileChanged(const QString & fileName);
};

#endif
