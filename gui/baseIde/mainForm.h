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

#ifndef MAIN_FORM_H_
#define MAIN_FORM_H_

#include <QMainWindow>
#include <QList>
#include <QPair>

class QAction;
class QTabWidget;
class QProcess;
class QLabel;
class QProgressBar;

class QCloseEvent;
class QDragEnterEvent;
class QDropEvent;
class QMoveEvent;
class QResizeEvent;

namespace divine {
namespace gui {

class ModuleManager;

class RecentFilesMenu;
class PreferencesDialog;

class AbstractToolLock;
class AbstractEditor;
class AbstractDocument;

class LayoutManager;
class SimulationProxy;

/*!
 * The MainForm implements the main window of the application.
 */
class MainForm : public QMainWindow
{
    Q_OBJECT

  public:
    static QIcon loadIcon(const QString & path);
    static QString getDropPath(QDropEvent * event);

  public:
    MainForm(QWidget * parent = 0, Qt::WindowFlags flags = 0);
    ~MainForm();

    // call after modules are loaded
    void initialize();

    // access to internal components for modules
    ModuleManager * modules() {return modules_;}
    PreferencesDialog * preferences() {return preferences_;}
    LayoutManager * layouts() {return layman_;}
    SimulationProxy * simulation() {return simulation_;}

    // locking
    bool isLocked() const {return lock_ != NULL;}
    bool isLocked(AbstractToolLock * lck) const {return lock_ == lck;}
    bool isLocked(AbstractDocument * lck) const;
    bool lock(AbstractToolLock * lck);
    bool release(AbstractToolLock * lck);

    // editor and document access
    QList<AbstractDocument*> documents();
    AbstractEditor * activeEditor();

    // auto save/stop functions. queries user if necessary
    bool maybeStop(AbstractEditor * editor);
    bool maybeStopAll();
    bool maybeSave(AbstractEditor * editor);
    bool maybeSave(const QList<AbstractEditor*> & editors);
    bool maybeSaveAll();

  public slots:
    bool openDocument(QString path, QStringList views = QStringList(), bool focus = true);

    void addEditor(AbstractEditor * editor);
    void activateEditor(AbstractEditor * editor);
    bool closeEditor(AbstractEditor * editor = NULL);
    bool closeEditor(const QList<AbstractEditor*> & editors);

    void updateTabLabel(AbstractEditor * editor);
    void updateWindowTitle();

    void updateActions();

  signals:
    //! Notifies plugins that application settings have been changed.
    void settingsChanged();

    void lockChanged();

    void editorActivated(AbstractEditor * editor);
    void editorDeactivated();
    void editorClosed(AbstractEditor * editor);

    //! Forwards message signals from other components.
    void message(const QString & msg);

  protected:
    void closeEvent(QCloseEvent * event);
    void dragEnterEvent(QDragEnterEvent * event);
    void dropEvent(QDropEvent * event);
    void moveEvent(QMoveEvent * event);
    void resizeEvent(QResizeEvent * event);

  private:
    RecentFilesMenu * recentMenu_;

    QAction * saveAct_;
    QAction * saveAsAct_;
    QAction * saveAllAct_;
    QAction * reloadAct_;
    QAction * reloadAllAct_;
    QAction * printAct_;
    QAction * previewAct_;
    QAction * closeAct_;
    QAction * closeAllAct_;

    QTabWidget * pageArea_;

    // current simulator
    SimulationProxy * simulation_;

    // registered modules
    ModuleManager * modules_;

    // layouts
    LayoutManager * layman_;

    // persistent dialogs
    PreferencesDialog * preferences_;

    // misc persistent objects
    QProcess * helpProc_;

    // busy state (loading or other operation in progress)
    AbstractToolLock * lock_;

  private:
    void createActions();
    void createMenus();
    void createToolBars();
    void createStatusBar();

    void restoreSession();
    void saveSession();

  private slots:
    void newDocument();
    void open();
    void openRecent(QAction * action);
    bool save(AbstractEditor * editor = NULL);
    bool save(const QList<AbstractEditor*> & editors);
    bool saveAs(AbstractEditor * editor = NULL);
    void saveAll();
    void reload(AbstractEditor * editor = NULL);
    void reloadAll();
    void print();
    void preview();
    void closeAll();
    void showPreferences();
    void help();
    void about();

    void onEditorModified(AbstractEditor * editor, bool state);
    void onPrintRequested(QPrinter * printer);
    void onTabCurrentChanged(int index);
    void onTabCloseRequested(int index);
};

}
}

#endif
