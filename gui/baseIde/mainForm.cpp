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

#include <QtGui>

#include "preferencesDialog.h"
#include "multiSaveDialog.h"
#include "newDocumentDialog.h"

#include "mainForm.h"
#include "simulationProxy.h"
#include "abstractEditor.h"
#include "abstractDocument.h"
#include "abstractToolLock.h"
#include "settings.h"
#include "layoutManager.h"
#include "recentFilesMenu.h"
#include "modules.h"
#include "moduleManager.h"

#ifdef Q_OS_WIN32
#  define ASSISTANT_BIN "assistant.exe"
#else
#  define ASSISTANT_BIN "assistant"
#endif

namespace divine {
namespace gui {

//
// MainForm
//
QIcon MainForm::loadIcon(const QString & path)
{
  QIcon icon;

  QDir iconDir(path);

  foreach(QString entry, iconDir.entryList())
  {
    icon.addFile(path + "/" + entry, QSize(), QIcon::Normal, QIcon::On);
  }
  return icon;
}

// to keep the behaviour consistent among editors
QString MainForm::getDropPath(QDropEvent * event)
{
  if (event->mimeData()->hasUrls()) {
    Q_ASSERT(!event->mimeData()->urls().isEmpty());
    QUrl url = event->mimeData()->urls().first();

    if (url.scheme() == "file") {
      QString path = url.path();
// check for invalid paths (/C:/blabla)
#ifdef Q_OS_WIN32
      QRegExp drive_re("^/[A-Z]:");
      if(path.contains(drive_re))
        path.remove(0, 1);
#endif
      return path;
    }
  }
  return QString();
}

MainForm::MainForm(QWidget* parent, Qt::WindowFlags flags)
    : QMainWindow(parent, flags), simulation_(NULL), lock_(NULL)
{
  setObjectName("ide.main");
  setWindowIcon(QIcon(":/icons/divine"));

  modules_ = new ModuleManager();

  QStringList layouts;
  layouts << "edit" << "debug";
  layman_ = new LayoutManager(this, layouts);

  pageArea_ = new QTabWidget(this);
  pageArea_->setMovable(true);
  pageArea_->setTabsClosable(true);
  pageArea_->setDocumentMode(true);
  setCentralWidget(pageArea_);

  connect(pageArea_, SIGNAL(tabCloseRequested(int)),
          SLOT(onTabCloseRequested(int)));
  connect(pageArea_, SIGNAL(currentChanged(int)),
          SLOT(onTabCurrentChanged(int)));

  // persistent objects
  preferences_ = new PreferencesDialog(this);
  helpProc_ = new QProcess(this);

  // create actions and menus
  createActions();
  createMenus();
  createToolBars();
  createStatusBar();

  simulation_ = new SimulationProxy(this);

  // init action state
  updateActions();

  setAcceptDrops(true);
}

MainForm::~MainForm()
{
}

/*!
 * Finishes initialization of the main window. It should be
 * called after all plugins are loaded.
 */
void MainForm::initialize()
{
  QByteArray geometry =
    Settings(this).value("geometry", QByteArray()).toByteArray();

  if (!geometry.isEmpty()) {
    restoreGeometry(geometry);
  } else {
    resize(500, 400);
    move(200, 150);
  }

  layman_->readSettings();

  restoreSession();

  updateWindowTitle();

  layman_->switchLayout("edit");
}

bool MainForm::isLocked(AbstractDocument * lck) const
{
  return lock_ && lock_->isLocked(lck);
}

bool MainForm::lock(AbstractToolLock * lck)
{
  // can't have two locks
  if (lock_ != NULL)
    return false;

  lock_ = lck;

  emit lockChanged();
  return true;
}

bool MainForm::release(AbstractToolLock * key)
{
  if (lock_ != key)
    return false;

  lock_ = NULL;

  emit lockChanged();
  return true;
}

QList<AbstractDocument*> MainForm::documents()
{
  QSet<AbstractDocument*> docs;
  
  for (int i = 0; i < pageArea_->count(); ++i) {
    AbstractEditor * editor = qobject_cast<AbstractEditor*>(pageArea_->widget(i));

    if (editor)
      docs.insert(editor->document());
  }
  return docs.toList();
}

AbstractEditor * MainForm::activeEditor()
{
  AbstractEditor * editor = qobject_cast<AbstractEditor*>(pageArea_->currentWidget());
  return editor;
}

/*!
 * If there is a running simulation (or other operation), attempts to stop it.
 * Notifies user of the operation (and allows him to abort it).
 * \return Success if the operation wasn't cancelled by the user.
 */
bool MainForm::maybeStop(AbstractEditor * editor)
{
  // nothing running
  if (!isLocked())
    return true;

  if (!editor)
    editor = qobject_cast<AbstractEditor*>(pageArea_->currentWidget());

  // cannot be locked without an editor
  Q_ASSERT(editor);

  // is the current document locked?
  if (!isLocked(editor->document()))
    return true;

  return lock_->maybeRelease();
}

bool MainForm::maybeStopAll()
{
  AbstractEditor * editor;

  for (int i = 0; i < pageArea_->count(); ++i) {
    editor = qobject_cast<AbstractEditor*>(pageArea_->widget(i));

    if (!maybeStop(editor))
      return false;
  }
  return true;
}

/*!
 * Attempts to automatically save given editor. If the editor is new (untitled)
 * asks user for a new name.
 *
 * \note We are working with editors and not documents here, since we want to
 * keep the possibility of having a document spanning different files.
 *
 * \return Success if the operation wasn't cancelled by the user.
 */
bool MainForm::maybeSave(AbstractEditor * editor)
{
  if (!editor->isModified())
    return true;

  int ret = QMessageBox::warning(
              this, tr("DiVinE IDE"),
              tr("Document contains unsaved changes. Do you wish to save them?"),
              QMessageBox::Yes | QMessageBox::Default,
              QMessageBox::No, QMessageBox::Cancel | QMessageBox::Escape);

  if (ret == QMessageBox::Cancel)
    return false;

  if (ret == QMessageBox::Yes && !save(editor))
    return false;

  return true;
}

/*!
 * Attempts to automatically save files in given editors. If the editor is
 * new (untitled) asks user for a new name.
 *
 * \note We are working with editors and not documents here, since we want to
 * keep the possibility of having a document spanning different files.
 *
 * \return Success if the operation wasn't cancelled by the user.
 * \see MultiSaveDialog
 */
bool MainForm::maybeSave(const QList<AbstractEditor*> & editors)
{
  QList<AbstractEditor*> tabs;

  // filter editor list
  foreach(AbstractEditor * editor, editors) {
    if (editor && editor->isModified()) {
      tabs.append(editor);
    }
  }

  if (tabs.isEmpty())
    return true;

  if (tabs.count() == 1 && pageArea_->currentWidget() == tabs.first()) {
    return maybeSave(tabs.first());
  } else {
    MultiSaveDialog dlg(tabs, this);

    if (!dlg.exec())
      return false;

    tabs = dlg.selection();

    foreach(AbstractEditor * editor, tabs) {
      if (!save(editor))
        return false;
    }
  }
  return true;
}

/*!
 * Attempts to automatically save all open editors.
 * \return Success if the operation wasn't cancelled by the user.
 */
bool MainForm::maybeSaveAll()
{
  QList<AbstractEditor*> tabs;

  for (int i = 0; i < pageArea_->count(); ++i) {
    tabs.append(qobject_cast<AbstractEditor*>(pageArea_->widget(i)));
  }

  return maybeSave(tabs);
}

/*!
 * Opens file specified by given path. If the file is already open, then
 * switches to it's editor.
 */
bool MainForm::openDocument(const QString path, QStringList views, bool focus)
{
  if (path.isEmpty())
    return false;

  QFileInfo finfo(path);

  if (!finfo.exists()) {
    QMessageBox::warning(this, tr("DiVinE IDE"), tr("File '%1' doesn't exist.").arg(path));
    return false;
  }

  AbstractDocument * doc = NULL;

  // isn't this document already open?
  for (int i = 0; i < pageArea_->count(); ++i) {
    AbstractEditor * editor = qobject_cast<AbstractEditor*>(pageArea_->widget(i));
    Q_ASSERT(editor);

    if (editor->document()->path() == finfo.absoluteFilePath()) {
      doc = editor->document();
      break;
    }
  }

  // open the document
  if (!doc) {
    const AbstractDocumentFactory * fab = modules_->findDocument(finfo.suffix());

    // universal editor
    if (!fab)
      fab = modules_->findDocument("");

    // nothing to open with
    if (!fab)
      return false;

    doc = fab->create(this);

    // false indicates fatal error, do not continue opening
    if(!doc->openFile(path)) {
      // cleanup
      doc->deleteLater();

      foreach(QString view, doc->views()) {
        AbstractEditor * ed = doc->editor(view);
        if(ed)
          ed->deleteLater();
      }
      return false;
    }
  }

  // if there's at least one in common, use it, otherwise open all
  QSet<QString> viewsDoc = QSet<QString>::fromList(doc->views());
  QSet<QString> viewsOp = QSet<QString>::fromList(views);

  viewsOp = viewsDoc.intersect(viewsOp);
  if (viewsOp.isEmpty())
    viewsOp = QSet<QString>::fromList(doc->views());

  // open all desired views
  foreach(QString view, viewsOp) {
    doc->openView(view);
  }

  // should we switch to document?
  if (focus) {
    pageArea_->setCurrentWidget(doc->editor(doc->mainView()));
  }
  recentMenu_->addFile(path);

  return true;
}

void MainForm::addEditor(AbstractEditor * editor)
{
  for (int i = 0; i < pageArea_->count(); ++i) {
    if (qobject_cast<AbstractEditor*>(pageArea_->widget(i)) == editor)
      return;
  }

  // FIXME: what if this happens several times? (open, close, open, close)
  connect(editor, SIGNAL(modificationChanged(AbstractEditor*,bool)),
          SLOT(onEditorModified(AbstractEditor*,bool)));
  pageArea_->addTab(editor, editor->viewName());
}

void MainForm::activateEditor(AbstractEditor * editor)
{
  int index = pageArea_->indexOf(editor);
  pageArea_->setCurrentIndex(index);
}

bool MainForm::closeEditor(AbstractEditor * editor)
{
  if (!editor)
    editor = qobject_cast<AbstractEditor*>(pageArea_->currentWidget());

  if (!editor)
    return false;

  if (editor->document()->viewCount() == 1 && !maybeStop(editor))
    return false;

  if (maybeSave(editor)) {
    pageArea_->removeTab(pageArea_->indexOf(editor));
    emit editorClosed(editor);
    return true;
  }
  return false;
}

bool MainForm::closeEditor(const QList<AbstractEditor*> & editors)
{
  foreach(AbstractEditor * editor, editors) {
    if (!maybeStop(editor))
      return false;
  }

  if (!maybeSave(editors))
    return false;

  foreach(AbstractEditor * editor, editors) {
    pageArea_->removeTab(pageArea_->indexOf(editor));
    emit editorClosed(editor);
  }
  return true;
}

void MainForm::updateTabLabel(AbstractEditor * editor)
{
  pageArea_->setTabText(pageArea_->indexOf(editor), editor->viewName());
}

//! Updates window title according to the active editor.
void MainForm::updateWindowTitle()
{
  AbstractEditor * editor = qobject_cast<AbstractEditor*>(pageArea_->currentWidget());

  if (editor) {
    // path/to/doc | Component 1
    QString title = editor->document()->path();

    if (!editor->viewName().isEmpty()) {
      if (!title.isEmpty())
        title.append(" | ");

      title.append(editor->viewName());
    }

    setWindowTitle(tr("DiVinE IDE - %1 [*]").arg(title));
    setWindowModified(editor->isModified());
  } else {
    setWindowTitle(tr("DiVinE IDE [*]"));
    setWindowModified(false);
  }
}

//! Updates basic actions according to the active editor.
void MainForm::updateActions()
{
  AbstractEditor * editor = qobject_cast<AbstractEditor*>(pageArea_->currentWidget());

  // if doc is modified
  saveAct_->setEnabled(editor);
  saveAsAct_->setEnabled(editor);
  saveAllAct_->setEnabled(editor);
  reloadAct_->setEnabled(editor);
  reloadAllAct_->setEnabled(editor);
  printAct_->setEnabled(editor);
  previewAct_->setEnabled(editor);
  closeAct_->setEnabled(editor);
  closeAllAct_->setEnabled(editor);
}

//! Reimplements QWidget::closeEvent.
void MainForm::closeEvent(QCloseEvent * event)
{
  if (maybeStopAll() && maybeSaveAll()) {
    // save settings
    saveSession();

    layman_->writeSettings();

    // close the application
    event->accept();
  } else {
    event->ignore();
  }
}

//! Reimplements QWidget::dragEnterEvent.
void MainForm::dragEnterEvent(QDragEnterEvent * event)
{
  if (event->mimeData()->hasUrls())
    event->acceptProposedAction();
  else
    QMainWindow::dragEnterEvent(event);
}

//! Reimplements QWidget::dropEvent.
void MainForm::dropEvent(QDropEvent * event)
{
  QString path = getDropPath(event);

  if (!path.isEmpty()) {
    openDocument(path);
  } else {
    QMainWindow::dropEvent(event);
  }
}

//! Reimplements QWidget::moveEvent.
void MainForm::moveEvent(QMoveEvent * event)
{
  Settings(this).setValue("geometry", saveGeometry());
}

//! Reimplements QWidget::resizeEvent.
void MainForm::resizeEvent(QResizeEvent * event)
{
  Settings(this).setValue("geometry", saveGeometry());
}

void MainForm::createActions()
{
  QAction * action;

  action = new QAction(loadIcon(":/icons/file/new"), tr("&New"), this);
  action->setObjectName("main.action.file.new");
  action->setShortcut(tr("Ctrl+N"));
  action->setStatusTip(tr("Create a new file"));
  connect(action, SIGNAL(triggered()), SLOT(newDocument()));

  action = new QAction(loadIcon(":/icons/file/open"), tr("&Open..."), this);
  action->setObjectName("main.action.file.open");
  action->setShortcut(tr("Ctrl+O"));
  action->setStatusTip(tr("Open existing file"));
  connect(action, SIGNAL(triggered()), SLOT(open()));

  saveAct_ = new QAction(loadIcon(":/icons/file/save"), tr("&Save"), this);
  saveAct_->setObjectName("main.action.file.save");
  saveAct_->setShortcut(tr("Ctrl+S"));
  saveAct_->setStatusTip(tr("Save the document to disk"));
  connect(saveAct_, SIGNAL(triggered()), SLOT(save()));

  saveAsAct_ = new QAction(loadIcon(":/icons/file/save-as"), tr("Save &As..."), this);
  saveAsAct_->setObjectName("main.action.file.saveAs");
  saveAsAct_->setStatusTip(tr("Save the document under a new name"));
  connect(saveAsAct_, SIGNAL(triggered()), SLOT(saveAs()));

  saveAllAct_ = new QAction(loadIcon(":/icons/file/save-all"), tr("Save A&ll"), this);
  saveAllAct_->setObjectName("main.action.file.saveAll");
  saveAllAct_->setStatusTip(tr("Save all documents to disk"));
  connect(saveAllAct_, SIGNAL(triggered()), SLOT(saveAll()));

  reloadAct_ = new QAction(loadIcon(":/icons/file/reload"), tr("&Reload"), this);
  reloadAct_->setObjectName("main.action.file.reload");
  reloadAct_->setStatusTip(tr("Reloads file discarding all changes"));
  connect(reloadAct_, SIGNAL(triggered()), SLOT(reload()));

  reloadAllAct_ = new QAction(tr("Reloa&d All"), this);
  reloadAllAct_->setObjectName("main.action.file.reloadAll");
  reloadAllAct_->setStatusTip(tr("Reloads all files"));
  connect(reloadAllAct_, SIGNAL(triggered()), SLOT(reloadAll()));

  printAct_ = new QAction(loadIcon(":/icons/file/print"), tr("&Print..."), this);
  printAct_->setObjectName("main.action.file.print");
  printAct_->setShortcut(tr("Ctrl+P"));
  printAct_->setStatusTip(tr("Prints current file"));
  connect(printAct_, SIGNAL(triggered()), SLOT(print()));

  previewAct_ = new QAction(loadIcon(":/icons/file/print-preview"), tr("Print Pre&view..."), this);
  previewAct_->setObjectName("main.action.file.printPreview");
  previewAct_->setStatusTip(tr("Shows print preview"));
  connect(previewAct_, SIGNAL(triggered()), SLOT(preview()));

  closeAct_ = new QAction(loadIcon(":/icons/file/close"), tr("&Close"), this);
  closeAct_->setObjectName("main.action.file.close");
  closeAct_->setShortcut(tr("Ctrl+W"));
  closeAct_->setStatusTip(tr("Close current document"));
  connect(closeAct_, SIGNAL(triggered()), SLOT(closeEditor()));

  closeAllAct_ = new QAction(tr("Cl&ose All"), this);
  closeAllAct_->setObjectName("main.action.file.closeAll");
  closeAllAct_->setStatusTip(tr("Close all documents"));
  connect(closeAllAct_, SIGNAL(triggered()), SLOT(closeEditor()));

  action = new QAction(loadIcon(":/icons/file/exit"), tr("E&xit"), this);
  action->setObjectName("main.action.file.exit");
  action->setShortcut(tr("Ctrl+Q"));
  action->setStatusTip(tr("Exit the application"));
  connect(action, SIGNAL(triggered()), SLOT(close()));

  action = new QAction(loadIcon(":/icons/edit/undo"), tr("&Undo"), this);
  action->setObjectName("main.action.edit.undo");
  action->setShortcut(tr("Ctrl+Z"));
  action->setStatusTip(tr("Undo the last action"));
  action->setEnabled(false);

  action = new QAction(loadIcon(":/icons/edit/redo"), tr("Re&do"), this);
  action->setObjectName("main.action.edit.redo");
  action->setShortcut(tr("Ctrl+Shift+Z"));
  action->setStatusTip(tr("Redo the last undone action"));
  action->setEnabled(false);

  action = new QAction(loadIcon(":/icons/edit/cut"), tr("Cu&t"), this);
  action->setObjectName("main.action.edit.cut");
  action->setShortcut(tr("Ctrl+X"));
  action->setStatusTip(tr("Cut the current selection's contents to the "
                           "clipboard"));
  action->setEnabled(false);

  action = new QAction(loadIcon(":/icons/edit/copy"), tr("&Copy"), this);
  action->setObjectName("main.action.edit.copy");
  action->setShortcut(tr("Ctrl+C"));
  action->setStatusTip(tr("Copy the current selection's contents to the "
                            "clipboard"));
  action->setEnabled(false);

  action = new QAction(loadIcon(":/icons/edit/paste"), tr("&Paste"), this);
  action->setObjectName("main.action.edit.paste");
  action->setShortcut(tr("Ctrl+V"));
  action->setStatusTip(tr("Paste the clipboard's contents into the current "
                             "selection"));
  action->setEnabled(false);

  action = new QAction(tr("&Overwrite Mode"), this);
  action->setObjectName("main.action.edit.overwrite");
  action->setShortcut(tr("Ins"));
  action->setCheckable(true);
  action->setStatusTip(tr("Toggle the overwrite mode"));
  action->setEnabled(false);

  action = new QAction(loadIcon(":/icons/edit/find"), tr("&Find..."), this);
  action->setObjectName("main.action.edit.find");
  action->setShortcut(tr("Ctrl+F"));
  action->setStatusTip(tr("Find specified text in the document"));
  action->setEnabled(false);

  action = new QAction(loadIcon(":/icons/edit/find-next"), tr("Find &Next"), this);
  action->setObjectName("main.action.edit.findNext");
  action->setShortcut(tr("F3"));
  action->setStatusTip(tr("Find the next occurence of specified text"));
  action->setEnabled(false);

  action = new QAction(loadIcon(":/icons/edit/find-prev"), tr("Find &Previous"), this);
  action->setObjectName("main.action.edit.findPrevious");
  action->setShortcut(tr("Shift+F3"));
  action->setStatusTip(tr("Find the previous occurence of the specified text"));
  action->setEnabled(false);

  action = new QAction(tr("&Replace..."), this);
  action->setObjectName("main.action.edit.replace");
  action->setShortcut(tr("Ctrl+R"));
  action->setStatusTip(tr("Replace"));
  action->setEnabled(false);

  action = new QAction(tr("&Dynamic Word Wrap"), this);
  action->setObjectName("main.action.view.dynamicWordWrap");
  action->setCheckable(true);
  action->setStatusTip(tr("Toggle dynamic word wrapping"));
  action->setEnabled(false);

  action = new QAction(tr("&Line Numbers"), this);
  action->setObjectName("main.action.view.lineNumbers");
  action->setCheckable(true);
  action->setStatusTip(tr("Toggle line numbers"));
  action->setEnabled(false);

  action = new QAction(loadIcon(":/icons/view/preferences"), tr("&Preferences..."), this);
  action->setObjectName("main.action.view.preferences");
  connect(action, SIGNAL(triggered()), SLOT(showPreferences()));

  action = new QAction(tr("&Metrics"), this);
  action->setObjectName("main.action.divine.metrics");
  action->setStatusTip(tr("State space metrics"));
  action->setEnabled(false);

  action = new QAction(tr("&Reachability"), this);
  action->setObjectName("main.action.divine.reachability");
  action->setStatusTip(tr("Searches for deadlock"));
  action->setEnabled(false);

  action = new QAction(tr("&Search..."), this);
  action->setObjectName("main.action.divine.search");
  action->setStatusTip(tr("Performs custom search"));
  action->setEnabled(false);

  action = new QAction(tr("&Verify"), this);
  action->setObjectName("main.action.divine.verify");
  action->setStatusTip(tr("Performs verification"));
  action->setEnabled(false);

  action = new QAction(tr("&Show Help"), this);
  action->setObjectName("main.action.help.showHelp");
  action->setShortcut(tr("F1"));
  action->setStatusTip(tr("Show the help window"));
  connect(action, SIGNAL(triggered()), SLOT(help()));

  action = new QAction(tr("&About"), this);
  action->setObjectName("main.action.help.about");
  action->setStatusTip(tr("Show the application's About box"));
  connect(action, SIGNAL(triggered()), SLOT(about()));

  action = new QAction(tr("About &Qt"), this);
  action->setObjectName("main.action.help.aboutQt");
  action->setStatusTip(tr("Show the Qt library's About box"));
  connect(action, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
}

void MainForm::createMenus()
{
  QMenu * menu;
  QAction * sep;

  menu = menuBar()->addMenu(tr("&File"));
  menu->setObjectName("main.menu.file");
  menu->addAction(findChild<QAction*>("main.action.file.new"));
  menu->addAction(findChild<QAction*>("main.action.file.open"));
  recentMenu_ = new RecentFilesMenu(tr("Open &Recent"), 5, menu);
  recentMenu_->setIcon(loadIcon(":/icons/file/open-recent"));
  menu->addMenu(recentMenu_);
  connect(menu, SIGNAL(triggered(QAction *)), SLOT(openRecent(QAction *)));
  sep = menu->addSeparator();
  sep->setObjectName("main.action.file.separator1");
  menu->addAction(saveAct_);
  menu->addAction(saveAsAct_);
  menu->addAction(saveAllAct_);
  menu->addAction(reloadAct_);
  menu->addAction(reloadAllAct_);
  sep = menu->addSeparator();
  sep->setObjectName("main.action.file.separator2");
  menu->addAction(printAct_);
  menu->addAction(previewAct_);
  sep = menu->addSeparator();
  sep->setObjectName("main.action.file.separator4");
  menu->addAction(closeAct_);
  menu->addAction(closeAllAct_);
  sep = menu->addSeparator();
  sep->setObjectName("main.action.file.separator5");

  menu->addAction(findChild<QAction*>("main.action.file.exit"));

  menu = menuBar()->addMenu(tr("&Edit"));
  menu->setObjectName("main.menu.edit");
  menu->addAction(findChild<QAction*>("main.action.edit.undo"));
  menu->addAction(findChild<QAction*>("main.action.edit.redo"));
  sep = menu->addSeparator();
  sep->setObjectName("main.action.edit.separator1");
  menu->addAction(findChild<QAction*>("main.action.edit.cut"));
  menu->addAction(findChild<QAction*>("main.action.edit.copy"));
  menu->addAction(findChild<QAction*>("main.action.edit.paste"));
  sep = menu->addSeparator();
  sep->setObjectName("main.action.edit.separator2");
  menu->addAction(findChild<QAction*>("main.action.edit.overwrite"));
  sep = menu->addSeparator();
  sep->setObjectName("main.action.edit.separator3");
  menu->addAction(findChild<QAction*>("main.action.edit.find"));
  menu->addAction(findChild<QAction*>("main.action.edit.findNext"));
  menu->addAction(findChild<QAction*>("main.action.edit.findPrevious"));
  menu->addAction(findChild<QAction*>("main.action.edit.replace"));

  menu = menuBar()->addMenu(tr("&View"));
  menu->setObjectName("main.menu.view");

  QMenu * sub = menu->addMenu("&Toolbars");
  sub->setObjectName("main.menu.view.toolbars");
  sub = menu->addMenu("&Panels");
  sub->setObjectName("main.menu.view.panels");
  sep = menu->addSeparator();
  sep->setObjectName("main.action.view.separator1");
  menu->addAction(findChild<QAction*>("main.action.view.dynamicWordWrap"));
  menu->addAction(findChild<QAction*>("main.action.view.lineNumbers"));
  sep = menu->addSeparator();
  sep->setObjectName("main.action.view.separator2");
  menu->addAction(findChild<QAction*>("main.action.view.preferences"));

  menu = menuBar()->addMenu(tr("&Tools"));
  menu->setObjectName("main.menu.tools");
  menu->addAction(findChild<QAction*>("main.action.divine.metrics"));
  menu->addAction(findChild<QAction*>("main.action.divine.reachability"));
  menu->addAction(findChild<QAction*>("main.action.divine.search"));
  menu->addAction(findChild<QAction*>("main.action.divine.verify"));

  menu = menuBar()->addMenu(tr("Si&mulate"));
  menu->setObjectName("main.menu.simulate");

  sep = menuBar()->addSeparator();
  sep->setObjectName("main.action.separator1");

  menu = menuBar()->addMenu(tr("&Help"));
  menu->setObjectName("main.menu.help");
  menu->addAction(findChild<QAction*>("main.action.help.showHelp"));
  sep = menu->addSeparator();
  sep->setObjectName("main.action.help.separator1");
  menu->addAction(findChild<QAction*>("main.action.help.about"));
  menu->addAction(findChild<QAction*>("main.action.help.aboutQt"));
}

void MainForm::createToolBars()
{
  QToolBar * toolbar;
  QAction * action;

  QMenu * menu = findChild<QMenu*>("main.menu.view.toolbars");
  Q_ASSERT(menu);

  toolbar = addToolBar(tr("File"));
  toolbar->setObjectName("main.toolbar.file");
  toolbar->addAction(findChild<QAction*>("main.action.file.new"));
  toolbar->addAction(findChild<QAction*>("main.action.file.open"));
  toolbar->addAction(saveAct_);

  action = toolbar->toggleViewAction();
  action->setText(tr("&File"));
  action->setStatusTip(tr("Toggles the file toolbar"));
  menu->addAction(action);

  toolbar = addToolBar(tr("Edit"));
  toolbar->setObjectName("main.toolbar.edit");
  toolbar->addAction(findChild<QAction*>("main.action.edit.undo"));
  toolbar->addAction(findChild<QAction*>("main.action.edit.redo"));
  toolbar->addAction(findChild<QAction*>("main.action.edit.cut"));
  toolbar->addAction(findChild<QAction*>("main.action.edit.copy"));
  toolbar->addAction(findChild<QAction*>("main.action.edit.paste"));

  action = toolbar->toggleViewAction();
  action->setText(tr("&Edit"));
  action->setStatusTip(tr("Toggles the edit toolbar"));
  menu->addAction(action);
}

void MainForm::createStatusBar()
{
  statusBar()->setSizeGripEnabled(true);

  QPushButton * button = new QPushButton(statusBar());
  button->setObjectName("main.status.stop");
  button->setIcon(loadIcon(":/icons/common/stop"));
  button->setFlat(true);
  button->setFixedSize(22, 22);
  button->setVisible(false);
  statusBar()->addPermanentWidget(button);

  QProgressBar * progress = new QProgressBar(statusBar());
  progress->setObjectName("main.status.progress");
  progress->setMaximum(0);
  progress->setMinimum(0);
  progress->setMaximumWidth(120);
  progress->setVisible(false);
  statusBar()->addPermanentWidget(progress);

  QLabel * label = new QLabel(statusBar());
  label->setObjectName("main.status.label");
  label->setVisible(false);
  statusBar()->addPermanentWidget(label);

  statusBar()->showMessage(tr("Ready"));
}

void MainForm::restoreSession()
{
  Settings s("ide/session");

  int size = s.beginReadArray("pages");

  for (int i = 0; i < size; ++i) {
    s.setArrayIndex(i);

    QString document = s.value("document", QString()).toString();
    QString view = s.value("view", QString()).toString();

    if (document.isEmpty() || view.isEmpty())
      continue;

    openDocument(document, QStringList(view), false);
  }

  s.endArray();

  int page = s.value("activePage", 0).toInt();
  pageArea_->setCurrentIndex(page);
}

void MainForm::saveSession()
{
  Settings s("ide/session");

  s.beginWriteArray("pages");

  for (int i = 0; i < pageArea_->count(); ++i) {
    AbstractEditor * editor = qobject_cast<AbstractEditor*>(pageArea_->widget(i));

    Q_ASSERT(editor);

    s.setArrayIndex(i);
    s.setValue("document", editor->document()->path());
    s.setValue("view", editor->viewName());
  }
  s.endArray();

  s.setValue("activePage", pageArea_->currentIndex());
}

void MainForm::newDocument()
{
  NewDocumentDialog dlg(modules_->documents(), this);

  if (!dlg.exec())
    return;

  const AbstractDocumentFactory * factory = modules_->findDocument(dlg.selection());
  Q_ASSERT(factory);

  AbstractDocument * doc = factory->create(this);

  foreach(QString view, doc->views()) {
    doc->openView(view);
  }

  pageArea_->setCurrentWidget(doc->editor(doc->mainView()));
}

void MainForm::open()
{
  QStringList filters;
  QStringList suffixes;

  const ModuleManager::DocumentList & docs = modules_->documents();

  foreach(ModuleManager::DocumentList::value_type itr, docs) {
    // skip universal editor
    if(itr->suffix().isEmpty())
      continue;

    suffixes << "*." + itr->suffix();
  }

  filters << QString("%1 (%2) (%2)").arg(tr("All applicable files"), suffixes.join(" "));

  foreach(ModuleManager::DocumentList::value_type itr, docs) {
    // skip universal editor
    if(itr->suffix().isEmpty())
      continue;

    filters << QString("%1 (*.%2) (*.%2)").arg(itr->filter(), itr->suffix());
  }

  QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"),
                     "", filters.join(";;"));

  openDocument(fileName);
}

void MainForm::openRecent(QAction * action)
{
  Q_ASSERT(action);

  // if action does not contain a filename, toString() will return empty string
  openDocument(action->data().toString());
}

bool MainForm::save(AbstractEditor * editor)
{
  if (!editor)
    editor = qobject_cast<AbstractEditor*>(pageArea_->currentWidget());

  if (!editor)
    return true;

  QString path = editor->document()->path();

  if (path.isEmpty())
    return saveAs(editor);

  return editor->saveFile(path);
}

bool MainForm::save(const QList<AbstractEditor*> & editors)
{
  foreach(AbstractEditor * editor, editors) {
    Q_ASSERT(editor);

    if (editor->isModified())
      save(editor);
  }
}

bool MainForm::saveAs(AbstractEditor * editor)
{
  if (!editor)
    editor = qobject_cast<AbstractEditor*>(pageArea_->currentWidget());

  if (!editor)
    return true;

  // setup the file dialog
  QString filter;

  const AbstractDocumentFactory * fab = modules_->findDocument(
    QFileInfo(editor->sourcePath()).suffix());

  if(!fab)
    fab = modules_->findDocument("");

  // the current editor had to be created somehow
  Q_ASSERT(fab);

  if (fab->suffix().isEmpty()) {
    filter = tr("All files (*)");
  } else {
    filter = QString("%1 (*.%2) (*.%2)").arg(fab->filter(), fab->suffix());
  }

  QString fileName = QFileDialog::getSaveFileName(this, tr("Save File"), "", filter);

  // just a normal save then
  if (fileName == editor->sourcePath())
    return editor->saveFile(fileName);

  // overwriting existing file
  if (QFileInfo(fileName).exists()) {
    QList<AbstractEditor*> editors;
    for (int i = 0; i < pageArea_->count(); ++i) {
      AbstractEditor * ed = qobject_cast<AbstractEditor*>(pageArea_->currentWidget());
      Q_ASSERT(ed);

      if (ed->document()->path() == QFileInfo(fileName).absoluteFilePath())
        editors.append(ed);
    }

    // should not be empty
    if (!editors.isEmpty()) {
      if (!closeEditor(editors))
        return false;
    }
  // otherwise try to correct suffix
  } else if (QFileInfo(fileName).suffix().isEmpty() && !fab->suffix().isEmpty()) {
    fileName.append(".");
    fileName.append(fab->suffix());
  }

  if(!editor->saveFile(fileName))
     return false;

  // update recent files
  recentMenu_->addFile(editor->document()->path());
  return true;
}

void MainForm::saveAll()
{
  for (int i = 0; i < pageArea_->count(); ++i) {
    AbstractEditor * editor = qobject_cast<AbstractEditor*>(pageArea_->widget(i));
    Q_ASSERT(editor);

    if (editor->isModified())
      save(editor);
  }
}

void MainForm::reload(AbstractEditor * editor)
{
  if (!editor)
    editor = qobject_cast<AbstractEditor*>(pageArea_->currentWidget());

  Q_ASSERT(editor);

  editor->reload();
}

void MainForm::reloadAll()
{
  for (int i = 0; i < pageArea_->count(); ++i) {
    AbstractEditor * editor = qobject_cast<AbstractEditor*>(pageArea_->widget(i));
    Q_ASSERT(editor);

    reload(editor);
  }
}

void MainForm::print()
{
  AbstractEditor * editor = qobject_cast<AbstractEditor*>(pageArea_->currentWidget());
  Q_ASSERT(editor);

  QPrinter printer;

  QPrintDialog dlg(&printer, this);
  dlg.setWindowTitle(tr("Print Document"));

  if (dlg.exec() != QDialog::Accepted)
    return;

  editor->print(&printer);
}

void MainForm::preview()
{
  AbstractEditor * editor = qobject_cast<AbstractEditor*>(pageArea_->currentWidget());
  Q_ASSERT(editor);

  QPrintPreviewDialog dlg(this);

  connect(&dlg, SIGNAL(paintRequested(QPrinter*)), SLOT(onPrintRequested(QPrinter*)));

  dlg.exec();
}

void MainForm::closeAll()
{
  if (maybeStopAll() && maybeSaveAll()) {
    while (pageArea_->count()) {
      AbstractEditor * editor = qobject_cast<AbstractEditor*>(pageArea_->currentWidget());

      pageArea_->removeTab(pageArea_->currentIndex());
      emit editorClosed(editor);
    }
  }
}

//! Shows the references dialog.
void MainForm::showPreferences()
{
  preferences_->initialize();

  if (preferences_->exec())
    emit settingsChanged();
}

void MainForm::help()
{
  if (helpProc_->state() == QProcess::Running) {
// NOTE: remote focus won't be implemented in the near future :(
//       QByteArray ar;
//       ar.append("focus");
//       ar.append('\0');
//       helpProc_->write(ar);
  } else {
    const char * envPlugins = getenv("DIVINE_GUI_HELP_PATH");
    QString helpdir("help");

    if(envPlugins)
      helpdir = envPlugins;

    helpProc_->start(QString("%1 -enableRemoteControl -collectionFile %2/divine.qhc").arg(ASSISTANT_BIN, helpdir),
                     QIODevice::WriteOnly);
    helpProc_->waitForStarted();

    if (helpProc_->state() != QProcess::Running) {
      QMessageBox::warning(this, tr("DiVinE IDE"), tr("Error launching help viewer!"));
    }
  }
}

//! Displays the application's about box
void MainForm::about()
{
  QMessageBox::about(this, tr("About DiVinE IDE"),
                     tr("<b>DiVinE IDE</b> is a development environment for DiViNE LTL model"
                        "checking system.<br><br>Project web page: "
                        "<a href=\"http://divine.fi.muni.cz\">http://divine.fi.muni.cz</a>"));
}

//! Updates modification notifications on given editor
void MainForm::onEditorModified(AbstractEditor * editor, bool state)
{
  int tab = pageArea_->indexOf(editor);

  pageArea_->setTabIcon(tab, state ? loadIcon(":/icons/file/save") : QIcon());

  if (editor == pageArea_->currentWidget())
    setWindowModified(state);
}

void MainForm::onPrintRequested(QPrinter * printer)
{
  AbstractEditor * editor = qobject_cast<AbstractEditor*>(pageArea_->currentWidget());

  if (!editor)
    return;

  editor->print(printer);
}

void MainForm::onTabCurrentChanged(int index)
{
  emit editorDeactivated();

  AbstractEditor * editor = qobject_cast<AbstractEditor*>(pageArea_->currentWidget());

  if (editor)
    emit editorActivated(editor);

  updateWindowTitle();

  updateActions();
}

void MainForm::onTabCloseRequested(int index)
{
  AbstractEditor * editor = qobject_cast<AbstractEditor*>(pageArea_->widget(index));

  if (editor)
    closeEditor(editor);
}

}
}
