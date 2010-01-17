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

#include "preferences_dlg.h"
#include "multisave_dlg.h"
#include "search_dlg.h"

#include "mainform.h"
#include "simulator.h"
#include "editor.h"
#include "settings.h"
#include "layout.h"
#include "recent.h"
#include "plugins.h"

#ifdef Q_OS_WIN32
#  define ASSISTANT_BIN "assistant.exe"
#else
#  define ASSISTANT_BIN "assistant"
#endif

//
// MainForm
//
MainForm::MainForm(QWidget* parent, Qt::WindowFlags flags)
    : QMainWindow(parent, flags), simProxy_(NULL)
{
  QStringList layouts;
  layouts << "edit" << "debug";
  layman_ = new LayoutManager(this, layouts);

  pageArea_ = new QTabWidget(this);
  pageArea_->setMovable(true);
  pageArea_->setTabsClosable(true);
//   pageArea_->setDocumentMode(true);
  setCentralWidget(pageArea_);

  connect(pageArea_, SIGNAL(tabCloseRequested(int)),
          SLOT(onTabCloseRequested(int)));
  connect(pageArea_, SIGNAL(currentChanged(int)),
          SLOT(onTabCurrentChanged(int)));
          
  // persistent dialogs
  preferences_ = new PreferencesDialog(this);
  search_ = new SearchDialog(this);

  connect(search_, SIGNAL(findNext()), SLOT(findNext()));
  connect(search_, SIGNAL(replace()), SLOT(replace()));
  connect(search_, SIGNAL(replaceAll()), SLOT(replaceAll()));

  // other persistent objects
  helpProc_ = new QProcess(this);
  watcher_ = new QFileSystemWatcher(this);
  
  randomTimer_ = new QTimer(this);
  randomTimer_->setSingleShot(false);

  connect(watcher_, SIGNAL(fileChanged(QString)), SLOT(onFileChanged(QString)));

  // create actions and menus
  createActions();
  createMenus();
  createToolBars();
  createStatusBar();

  // init action state
  updateWorkspace();
  updateSimulator();

  setAcceptDrops(true);
}

MainForm::~MainForm()
{
}

/*!
 * Finishes initialization of the main window. It should be
 * called after all plugins are loaded.
 */
void MainForm::initialize(void)
{
  // read app settings (recent files etc.)
  readSettings();
  updateWindowTitle();

  layman_->switchLayout("edit");
}

/*!
 * Registers EditorBuilder instance for given document type.
 * \param suffix Document suffix.
 * \param filter Filter for file dialogs.
 * \param builder EditorBuilder instance.
 */
void MainForm::registerDocument
(const QString & suffix, const QString & filter, EditorBuilder * builder)
{
  // check for duplicit suffixes
  foreach(BuilderList::value_type itr, docBuilders_) {
    if (itr.first.first == suffix)
      return;
  }

  BuilderPair entry = BuilderPair(DocTypeInfo(suffix, filter), builder);

  docBuilders_.append(entry);
  qSort(docBuilders_.begin(), docBuilders_.end());
}

/*!
 * Registers SimulatorFactory instance for given document type.
 * \param suffix Document suffix.
 * \param factory SimulatorFactory instance.
 */
void MainForm::registerSimulator(const QString & suffix, SimulatorFactory * factory)
{
  // no such thing as default simulator or null loader
  if (suffix.isEmpty() || !factory)
    return;

  // same behaviour as doc loaders
  if (simLoaders_.contains(suffix))
    return;

  simLoaders_.insert(suffix, factory);
}

/*!
 * Registers a widget as a preferences page.
 * \param group Settings group.
 * \param tab Tab name.
 * \param page New page.
 * \see PreferencesDialog
 * \see PreferencesPage
 */
void MainForm::registerPreferences
(const QString & group, const QString & tab, QWidget * page)
{
  preferences_->addWidget(group, tab, page);
}

//! Returns number of open editors.
int MainForm::editorCount(void)
{
  return pageArea_->count();
}

//! Returns active editor.
SourceEditor * MainForm::activeEditor(void)
{
  return qobject_cast<SourceEditor*>(pageArea_->currentWidget());
}

//! Returns editor with given file.
SourceEditor * MainForm::editor(const QString & path)
{
  SourceEditor * editor;

  for (int i = 0; i < pageArea_->count(); ++i) {
    editor = qobject_cast<SourceEditor*>(pageArea_->widget(i));

    QUrl url(editor->document()->metaInformation(QTextDocument::DocumentUrl));

    if (url.path() == QFileInfo(path).absoluteFilePath())
      return editor;
  }

  return NULL;
}

//! Returns editor with given id.
SourceEditor * MainForm::editor(int index)
{
  if (index < 0 || index >= pageArea_->count())
    return NULL;

  SourceEditor * editor = qobject_cast<SourceEditor*>(pageArea_->widget(index));

  return editor;
}

//! Returns list of opened files. Ignores unnamed (fresh new) files.
const QStringList MainForm::openedFiles(void) const
{
  QStringList res;
  SourceEditor * editor;

  for (int i = 0; i < pageArea_->count(); ++i) {
    editor = qobject_cast<SourceEditor*>(pageArea_->widget(i));

    QUrl url(editor->document()->metaInformation(QTextDocument::DocumentUrl));

    if(!url.path().isEmpty())
      res << url.path();
  }

  return res;
}

/*!
 * If there is a running simulation, attempts to stop it.
 * Notifies user of the operation (and allows him to abort it).
 * \return Success if the operation wasn't cancelled by the user.
 */
bool MainForm::maybeStop(void)
{
  const Simulator * simulator = simProxy_ ? simProxy_->simulator() : NULL;

  if (simulator && simulator->isRunning()) {
    int ret = QMessageBox::warning(this, tr("DiVinE IDE"),
                                   tr("The simulator is running.\n"
                                      "Do you want to abort the simulation?"),
                                   QMessageBox::Yes | QMessageBox::Default,
                                   QMessageBox::No | QMessageBox::Escape);

    if (ret != QMessageBox::Yes)
      return false;

    simProxy_->stop();
  }

  return true;
}

/*!
 * Attempts to automatically save given editor. If the editor is new (untitled)
 * asks user for a new name.
 * \return Success if the operation wasn't cancelled by the user.
 */
bool MainForm::maybeSave(SourceEditor * editor)
{
  if (!editor->document()->isModified())
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
 * Attempts to automatically save given editors. If the editor is new (untitled)
 * asks user for a new name.
 * \return Success if the operation wasn't cancelled by the user.
 * \see MultiSaveDialog
 */
bool MainForm::maybeSave(const QList<SourceEditor*> & editors)
{
  QList<SourceEditor*> tabs;

  // filter editor list
  foreach(SourceEditor * editor, editors) {
    if (editor && editor->document()->isModified()) {
      tabs.append(editor);
    }
  }

  if (tabs.isEmpty())
    return true;

  if (tabs.count() == 1 && pageArea_->currentWidget() == tabs.first()) {
    return maybeSave(tabs.first());
  } else {
    MultiSaveDialog * dlg = new MultiSaveDialog(tabs, this);

    if (!dlg->exec())
      return false;

    tabs = dlg->selection();

    foreach(SourceEditor * editor, tabs) {
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
bool MainForm::maybeSaveAll(void)
{
  QList<SourceEditor*> tabs;

  for (int i = 0; i < pageArea_->count(); ++i) {
    tabs.append(qobject_cast<SourceEditor*>(pageArea_->widget(i)));
  }

  return maybeSave(tabs);
}

/*!
 * Creates new file. Has no contents and no type, unless explicitly specified.
 * \param hint Document type of the initial contents (file suffix).
 * \param text Initial contents.
 */
void MainForm::newFile(const QString & hint, const QByteArray & text)
{
  SourceEditor * editor = new SourceEditor(this);

  connect(editor, SIGNAL(documentModified(SourceEditor*, bool)), SLOT(onDocumentModified(SourceEditor*, bool)));
  connect(editor, SIGNAL(cursorPositionChanged()), SLOT(updateStatusLabel()));
  connect(editor, SIGNAL(fileDropped(const QString&)), SLOT(openFile(const QString &)));
  connect(this, SIGNAL(settingsChanged()), editor, SLOT(readSettings()));
  connect(this, SIGNAL(highlightTransition(int)), editor, SLOT(highlightTransition(int)));

  connect(editor, SIGNAL(undoAvailable(bool)), undoAct_, SLOT(setEnabled(bool)));
  connect(editor, SIGNAL(redoAvailable(bool)), redoAct_, SLOT(setEnabled(bool)));
  connect(editor, SIGNAL(copyAvailable(bool)), cutAct_, SLOT(setEnabled(bool)));
  connect(editor, SIGNAL(copyAvailable(bool)), copyAct_, SLOT(setEnabled(bool)));

  editor->document()->setPlainText(text);
  editor->document()->setModified(!text.isEmpty());
  
  EditorBuilder * builder = NULL;
  
  // install hinted highlighter & completer
  if (!hint.isEmpty())
    builder = getBuilder(hint);

  if (builder) {
    editor->document()->setMetaInformation(QTextDocument::DocumentTitle, tr("untitled.%1").arg(hint));
    editor->document()->setMetaInformation(QTextDocument::DocumentUrl, QString("%1://").arg(hint));

    builder->install(editor);
  } else {
    editor->document()->setMetaInformation(QTextDocument::DocumentTitle, tr("untitled"));
    editor->document()->setMetaInformation(QTextDocument::DocumentUrl, QString(""));
  }
  
  const int tab = pageArea_->currentIndex();

  pageArea_->insertTab(tab + 1, editor, editor->documentTitle());
  pageArea_->setCurrentIndex(tab + 1);
  
  if(editor->document()->isModified())
    onDocumentModified(editor, true);
}

/*!
 * Opens file specified by given path. If the file is already open, then
 * switches to it's editor.
 */
void MainForm::openFile(const QString & path)
{
  if (path.isEmpty())
    return;

  QFileInfo finfo(path);

  if (!finfo.exists()) {
    QMessageBox::warning(this, tr("DiVinE IDE"), tr("File '%1' doesn't exist.").arg(path));
    return;
  }

  // isn't this file already open?
  for (int i = 0; i < pageArea_->count(); ++i) {
    SourceEditor * editor = qobject_cast<SourceEditor*>(pageArea_->widget(i));
    Q_ASSERT(editor);

    QUrl url(editor->document()->metaInformation(QTextDocument::DocumentUrl));

    if (url.path() == finfo.absoluteFilePath()) {
      pageArea_->setCurrentIndex(i);
      return;
    }
  }

  // open the document fileDropped
  SourceEditor * editor = new SourceEditor(this);

  if (!editor->loadFile(path)) {
    delete editor;
    return;
  }

  connect(editor, SIGNAL(documentModified(SourceEditor*, bool)), SLOT(onDocumentModified(SourceEditor*, bool)));

  connect(editor, SIGNAL(cursorPositionChanged()), SLOT(updateStatusLabel()));
  connect(editor, SIGNAL(fileDropped(const QString&)), SLOT(openFile(const QString &)));
  connect(this, SIGNAL(settingsChanged()), editor, SLOT(readSettings()));
  connect(this, SIGNAL(highlightTransition(int)), editor, SLOT(highlightTransition(int)));

  connect(editor, SIGNAL(undoAvailable(bool)), undoAct_, SLOT(setEnabled(bool)));
  connect(editor, SIGNAL(redoAvailable(bool)), redoAct_, SLOT(setEnabled(bool)));
  connect(editor, SIGNAL(copyAvailable(bool)), cutAct_, SLOT(setEnabled(bool)));
  connect(editor, SIGNAL(copyAvailable(bool)), copyAct_, SLOT(setEnabled(bool)));

  // update recent files
  recentMenu_->addFile(path);

  // create highlighter
  EditorBuilder * builder = getBuilder(finfo.suffix());

  QUrl url;
  url.setScheme(finfo.suffix());
  url.setPath(finfo.absoluteFilePath());
  
  editor->document()->setMetaInformation(QTextDocument::DocumentTitle, finfo.fileName());
  editor->document()->setMetaInformation(QTextDocument::DocumentUrl, url.toString());

  if (builder)
    builder->install(editor);

  // add to watcher
  watcher_->addPath(finfo.absoluteFilePath());

  // add new tab
  const int tab = pageArea_->currentIndex();

  pageArea_->insertTab(tab + 1, editor, editor->documentTitle());
  pageArea_->setCurrentIndex(tab + 1);
}

//! Starts the interactive simulation.
void MainForm::startSimulation(void)
{
  Q_ASSERT(!simProxy_);
  Q_ASSERT(pageArea_->currentWidget());

  SourceEditor * editor = qobject_cast<SourceEditor*>(pageArea_->currentWidget());
  QUrl url(editor->document()->metaInformation(QTextDocument::DocumentUrl));

  // if current page is untitled -> force save as
  if(url.path().isEmpty()) {
    if(!save(editor))
      return;
  }
  
  // auto save
  for(int i = 0; i < pageArea_->count(); ++i) {
    editor = qobject_cast<SourceEditor*>(pageArea_->widget(i));
    url = editor->document()->metaInformation(QTextDocument::DocumentUrl);
  
    if(editor->document()->isModified() && !url.path().isEmpty())
      save(editor);
  }
  
  editor = qobject_cast<SourceEditor*>(pageArea_->currentWidget());
  url = editor->document()->metaInformation(QTextDocument::DocumentUrl);
  
  SimulatorFactory * factory = simLoaders_[url.scheme()];
  Q_ASSERT(factory);

  Simulator * simulator = factory->load(url.path(), this);

  if (!simulator)
    return;
  
  // clear highlights
  for (int i = 0; i < pageArea_->count(); ++i) {
    editor = qobject_cast<SourceEditor*>(pageArea_->widget(i));
    editor->resetHighlighting();
  }
  
  // check for errors
  if(!simulator->isReady()) {
    checkSyntaxErrors(simulator);
    
    delete simulator;
    return;
  }

  layman_->switchLayout("debug");

  simProxy_ = new SimulatorProxy(simulator, this);
  
  // setup actions
  connect(restartAct_, SIGNAL(triggered()), simProxy_, SLOT(start()));
  connect(stopAct_, SIGNAL(triggered()), simProxy_, SLOT(stop()));
  connect(stepBackAct_, SIGNAL(triggered()), simProxy_, SLOT(undo()));
  connect(stepForeAct_, SIGNAL(triggered()), simProxy_, SLOT(redo()));
  
  connect(simProxy_, SIGNAL(message(QString)), SIGNAL(message(QString)));
  connect(simProxy_, SIGNAL(started()), SLOT(onSimulatorStarted()));
  connect(simProxy_, SIGNAL(stateChanged()), SLOT(updateSimulator()));
  connect(simProxy_, SIGNAL(stopped()), SLOT(onSimulatorStopped()));
  connect(simProxy_, SIGNAL(locked(bool)), SLOT(updateSimulator()));

  emit message(tr("=== Starting simulation ==="));
  emit simulatorChanged(simProxy_);

  // load settings
  sSettings().beginGroup("Simulator");

  if (sSettings().value("random", defSimulatorRandom).toBool()) {
    qsrand(sSettings().value("seed", defSimulatorSeed).toUInt());
  } else {
    qsrand(QTime::currentTime().msec());
  }

  sSettings().endGroup();

  // start the simulation
  simProxy_->start();

  QStringList files = simulator->files();

  for (int i = 0; i < pageArea_->count(); ++i) {
    editor = qobject_cast<SourceEditor*>(pageArea_->widget(i));
    QUrl url(editor->document()->metaInformation(QTextDocument::DocumentUrl));
  
    if (files.contains(url.path())) {
      editor->setReadOnly(true);
      editor->autoHighlight(simProxy_);
    }
  }

  updateSimulator();
  updateWorkspace();
}

//! Updates window title according to the active editor.
void MainForm::updateWindowTitle(void)
{
  SourceEditor * editor = qobject_cast<SourceEditor*>(pageArea_->currentWidget());

  if (editor) {
    // either real filename, or "untitled"
    QString title = editor->documentTitle();

    setWindowTitle(tr("DiVinE IDE - %1 [*]").arg(title));
    setWindowModified(editor->document()->isModified());
  } else {
    setWindowTitle(tr("DiVinE IDE [*]"));
    setWindowModified(false);
  }
}

//! Updates basic actions according to the active editor.
void MainForm::updateWorkspace(void)
{
  SourceEditor * editor = qobject_cast<SourceEditor*>(pageArea_->currentWidget());

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

  undoAct_->setEnabled(editor && editor->document()->isUndoAvailable() &&
                       !editor->isReadOnly());
  redoAct_->setEnabled(editor && editor->document()->isRedoAvailable() &&
                       !editor->isReadOnly());

  cutAct_->setEnabled(editor && editor->textCursor().hasSelection() &&
                      !editor->isReadOnly());
  copyAct_->setEnabled(editor && editor->textCursor().hasSelection());
  pasteAct_->setEnabled(editor && !editor->isReadOnly());

  overwriteAct_->setEnabled(editor);

  findAct_->setEnabled(editor);
  findNextAct_->setEnabled(editor);
  findPrevAct_->setEnabled(editor);
  replaceAct_->setEnabled(editor && !editor->isReadOnly());

  wordWrapAct_->setEnabled(editor);
  numbersAct_->setEnabled(editor);
}

//! Updates basic actions according to the active simulator.
void MainForm::updateSimulator(void)
{
  SourceEditor * editor = qobject_cast<SourceEditor*>(pageArea_->currentWidget());
  const Simulator * simulator = simProxy_ ? simProxy_->simulator() : NULL;

  QUrl url;
  if (editor)
    url = editor->document()->metaInformation(QTextDocument::DocumentUrl);

  const bool running = simulator && simulator->isRunning();
  const bool ready = editor && simLoaders_.contains(url.scheme());
  const bool locked = simProxy_ && simProxy_->isLocked();

  startAct_->setEnabled(!running && ready);
  stopAct_->setEnabled(running);
  restartAct_->setEnabled(running);
  stepBackAct_->setEnabled(running && !locked && simulator->canUndo());
  stepForeAct_->setEnabled(running && !locked && simulator->canRedo());
  stepRandomAct_->setEnabled(running && !locked && simulator->transitionCount());
  randomRunAct_->setEnabled(running && !locked && simulator->transitionCount());
  importAct_->setEnabled(!running && ready);
  exportAct_->setEnabled(running && !locked);
  
  // NOTE: cannot check syntax during simulation because the current dve
  //       simulator must be implemented as a singleton
  syntaxAct_->setEnabled(!running && ready);
  
}

//! Updates status label in the lower right corner of the window.
void MainForm::updateStatusLabel()
{
  SourceEditor * editor = qobject_cast<SourceEditor*>(pageArea_->currentWidget());

  if (!editor) {
    statusLabel_->setText("");
  } else {
    QTextCursor cur = editor->textCursor();

    statusLabel_->setText(tr("Line: %1 Col: %2").arg(QString::number(
                            cur.blockNumber() + 1), QString::number(cur.columnNumber() + 1)));
  }
}

//! Updates modification notifications on given editor
void MainForm::onDocumentModified(SourceEditor * editor, bool state)
{
  int tab = pageArea_->indexOf(editor);

  pageArea_->setTabIcon(tab, state ? QIcon(":/icons/file/save") : QIcon());

  if (editor == pageArea_->currentWidget())
    setWindowModified(state);
}

//! Reimplements QWidget::closeEvent.
void MainForm::closeEvent(QCloseEvent * event)
{
  if (maybeStop() && maybeSaveAll()) {
    // save settings
    writeSettings();

    search_->close();

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
  if (event->mimeData()->hasUrls()) {
    Q_ASSERT(!event->mimeData()->urls().isEmpty());
    QUrl url = event->mimeData()->urls().first();

    if (url.scheme() == "file") {
      QString path = event->mimeData()->urls().first().path();
// check for invalid paths (/C:/blabla)
#ifdef Q_OS_WIN32
      QRegExp drive_re("^/[A-Z]:");
      if(path.contains(drive_re))
        path.remove(0, 1);
#endif
      openFile(path);
    }
  } else {
    QMainWindow::dropEvent(event);
  }
}

void MainForm::createActions(void)
{
  newAct_ = new QAction(QIcon(":/icons/file/new"), tr("&New"), this);
  newAct_->setObjectName("newAct");
  newAct_->setShortcut(tr("Ctrl+N"));
  newAct_->setStatusTip(tr("Create a new file"));
  connect(newAct_, SIGNAL(triggered()), SLOT(newFile()));

  openAct_ = new QAction(QIcon(":/icons/file/open"), tr("&Open..."), this);
  openAct_->setObjectName("openAct");
  openAct_->setShortcut(tr("Ctrl+O"));
  openAct_->setStatusTip(tr("Open existing file"));
  connect(openAct_, SIGNAL(triggered()), SLOT(open()));

  saveAct_ = new QAction(QIcon(":/icons/file/save"), tr("&Save"), this);
  saveAct_->setObjectName("saveAct");
  saveAct_->setShortcut(tr("Ctrl+S"));
  saveAct_->setStatusTip(tr("Save the document to disk"));
  connect(saveAct_, SIGNAL(triggered()), SLOT(save()));

  saveAsAct_ = new QAction(tr("Save &As..."), this);
  saveAsAct_->setObjectName("saveAsAct");
  saveAsAct_->setStatusTip(tr("Save the document under a new name"));
  connect(saveAsAct_, SIGNAL(triggered()), SLOT(saveAs()));

  saveAllAct_ = new QAction(tr("Save A&ll"), this);
  saveAllAct_->setObjectName("saveAllAct");
  saveAllAct_->setStatusTip(tr("Save all documents to disk"));
  connect(saveAllAct_, SIGNAL(triggered()), SLOT(saveAll()));

  reloadAct_ = new QAction(tr("&Reload"), this);
  reloadAct_->setObjectName("reloadAct");
  reloadAct_->setStatusTip(tr("Reloads file discarding all changes"));
  connect(reloadAct_, SIGNAL(triggered()), SLOT(reload()));

  reloadAllAct_ = new QAction(tr("Reloa&d All"), this);
  reloadAllAct_->setObjectName("reloadAllAct");
  reloadAllAct_->setStatusTip(tr("Reloads all files"));
  connect(reloadAllAct_, SIGNAL(triggered()), SLOT(reloadAll()));

  printAct_ = new QAction(tr("&Print..."), this);
  printAct_->setObjectName("printAct");
  printAct_->setShortcut(tr("Ctrl+P"));
  printAct_->setStatusTip(tr("Prints current file"));
  connect(printAct_, SIGNAL(triggered()), SLOT(print()));

  previewAct_ = new QAction(tr("Print Pre&view..."), this);
  previewAct_->setObjectName("previewAct");
  previewAct_->setStatusTip(tr("Shows print preview"));
  connect(previewAct_, SIGNAL(triggered()), SLOT(preview()));

  closeAct_ = new QAction(tr("&Close"), this);
  closeAct_->setObjectName("closeAct");
  closeAct_->setShortcut(tr("Ctrl+W"));
  closeAct_->setStatusTip(tr("Close current document"));
  connect(closeAct_, SIGNAL(triggered()), SLOT(closeFile()));

  closeAllAct_ = new QAction(tr("Cl&ose All"), this);
  closeAllAct_->setObjectName("closeAllAct");
  closeAllAct_->setStatusTip(tr("Close all documents"));
  connect(closeAllAct_, SIGNAL(triggered()), SLOT(closeAll()));

  exitAct_ = new QAction(tr("E&xit"), this);
  exitAct_->setObjectName("exitAct");
  exitAct_->setShortcut(tr("Ctrl+Q"));
  exitAct_->setStatusTip(tr("Exit the application"));
  connect(exitAct_, SIGNAL(triggered()), SLOT(close()));

  undoAct_ = new QAction(QIcon(":/icons/edit/undo"), tr("&Undo"), this);
  undoAct_->setObjectName("undoAct");
  undoAct_->setShortcut(tr("Ctrl+Z"));
  undoAct_->setStatusTip(tr("Undo the last action"));

  redoAct_ = new QAction(QIcon(":/icons/edit/redo"), tr("Re&do"), this);
  redoAct_->setObjectName("redoAct");
  redoAct_->setShortcut(tr("Ctrl+Shift+Z"));
  redoAct_->setStatusTip(tr("Redo the last undone action"));

  cutAct_ = new QAction(QIcon(":/icons/edit/cut"), tr("Cu&t"), this);
  cutAct_->setObjectName("cutAct");
  cutAct_->setShortcut(tr("Ctrl+X"));
  cutAct_->setStatusTip(tr("Cut the current selection's contents to the "
                           "clipboard"));

  copyAct_ = new QAction(QIcon(":/icons/edit/copy"), tr("&Copy"), this);
  copyAct_->setObjectName("copyAct");
  copyAct_->setShortcut(tr("Ctrl+C"));
  copyAct_->setStatusTip(tr("Copy the current selection's contents to the "
                            "clipboard"));

  pasteAct_ = new QAction(QIcon(":/icons/edit/paste"), tr("&Paste"), this);
  pasteAct_->setObjectName("pasteAct");
  pasteAct_->setShortcut(tr("Ctrl+V"));
  pasteAct_->setStatusTip(tr("Paste the clipboard's contents into the current "
                             "selection"));

  overwriteAct_ = new QAction(tr("&Overwrite Mode"), this);
  overwriteAct_->setObjectName("overwriteAct");
  overwriteAct_->setShortcut(tr("Ins"));
  overwriteAct_->setCheckable(true);
  overwriteAct_->setStatusTip(tr("Toggle the overwrite mode"));
  connect(overwriteAct_, SIGNAL(toggled(bool)), SLOT(onOverwriteToggled(bool)));

  findAct_ = new QAction(tr("&Find..."), this);
  findAct_->setObjectName("findAct");
  findAct_->setShortcut(tr("Ctrl+F"));
  findAct_->setStatusTip(tr("Find specified text in the document"));
  connect(findAct_, SIGNAL(triggered()), SLOT(showFindDialog()));

  findNextAct_ = new QAction(tr("Find &Next"), this);
  findNextAct_->setObjectName("findNextAct");
  findNextAct_->setShortcut(tr("F3"));
  findNextAct_->setStatusTip(tr("Find the next occurence of specified text"));
  connect(findNextAct_, SIGNAL(triggered()), SLOT(findNext()));

  findPrevAct_ = new QAction(tr("Find &Previous"), this);
  findPrevAct_->setObjectName("findPrevAct");
  findPrevAct_->setShortcut(tr("Shift+F3"));
  findPrevAct_->setStatusTip(tr("Find the previous occurence of the specified text"));
  connect(findPrevAct_, SIGNAL(triggered()), SLOT(findPrevious()));

  replaceAct_ = new QAction(tr("&Replace..."), this);
  replaceAct_->setObjectName("replaceAct");
  replaceAct_->setShortcut(tr("Ctrl+R"));
  replaceAct_->setStatusTip(tr("Replace"));
  connect(replaceAct_, SIGNAL(triggered()), SLOT(showReplaceDialog()));

  wordWrapAct_ = new QAction(tr("&Dynamic Word Wrap"), this);
  wordWrapAct_->setObjectName("wordWrapAct");
  wordWrapAct_->setCheckable(true);
  wordWrapAct_->setStatusTip(tr("Toggle dynamic word wrapping"));
  connect(wordWrapAct_, SIGNAL(toggled(bool)), SLOT(onWordWrapToggled(bool)));

  numbersAct_ = new QAction(tr("&Line Numbers"), this);
  numbersAct_->setObjectName("numbersAct");
  numbersAct_->setCheckable(true);
  numbersAct_->setStatusTip(tr("Toggle line numbers"));
  connect(numbersAct_, SIGNAL(toggled(bool)), SLOT(onLineNumbersToggled(bool)));

  prefsAct_ = new QAction(tr("&Preferences..."), this);
  prefsAct_->setObjectName("prefsAct");
  connect(prefsAct_, SIGNAL(triggered()), SLOT(showPreferences()));

  startAct_ = new QAction(QIcon(":/icons/sim/start"), tr("&Start"), this);
  startAct_->setObjectName("startAct");
  startAct_->setShortcut(tr("F5"));
  startAct_->setStatusTip(tr("Start simulation"));
  connect(startAct_, SIGNAL(triggered()), SLOT(startSimulation()));

  stopAct_ = new QAction(QIcon(":/icons/sim/stop"), tr("Sto&p"), this);
  stopAct_->setObjectName("stopAct");
  stopAct_->setShortcut(tr("Shift+F5"));
  stopAct_->setStatusTip(tr("Stop the simulation"));

  restartAct_ = new QAction(QIcon(":/icons/sim/restart"),
                            tr("&Restart"), this);
  restartAct_->setObjectName("restartAct");
  restartAct_->setStatusTip(tr("Return to the initial state"));

  stepBackAct_ = new QAction(QIcon(":/icons/sim/undo"), tr("Step B&ack"), this);
  stepBackAct_->setObjectName("stepBackAct");
  stepBackAct_->setShortcut(tr("F6"));
  stepBackAct_->setStatusTip(tr("Undo last transition"));

  stepForeAct_ = new QAction(QIcon(":/icons/sim/redo"),
                             tr("Step &Forward"), this);
  stepForeAct_->setObjectName("stepForeAct");
  stepForeAct_->setShortcut(tr("F7"));
  stepForeAct_->setStatusTip(tr("Redo last transition"));

  stepRandomAct_ = new QAction(QIcon(":/icons/sim/random"),
                               tr("Ran&dom Step"), this);
  stepRandomAct_->setObjectName("stepRandomAct");
  stepRandomAct_->setShortcut(tr("F8"));
  stepRandomAct_->setStatusTip(tr("Choose and execute random transition"));
  connect(stepRandomAct_, SIGNAL(triggered()), SLOT(randomStep()));

  randomRunAct_ = new QAction(tr("&Random Run..."), this);
  randomRunAct_->setObjectName("randomRunAct");
  stepRandomAct_->setShortcut(tr("Ctrl+F8"));
  connect(randomRunAct_, SIGNAL(triggered()), SLOT(randomRun()));
  
  importAct_ = new QAction(tr("&Import..."), this);
  importAct_->setObjectName("importAct");
  importAct_->setStatusTip(tr("Import simulation run"));
  connect(importAct_, SIGNAL(triggered()), SLOT(importRun()));

  exportAct_ = new QAction(tr("&Export..."), this);
  exportAct_->setObjectName("exportAct");
  exportAct_->setStatusTip(tr("Export current simulation run"));
  connect(exportAct_, SIGNAL(triggered()), SLOT(exportRun()));

  syntaxAct_ = new QAction(tr("Check &Syntax"), this);
  syntaxAct_->setObjectName("syntaxAct");
  syntaxAct_->setStatusTip(tr("Check model syntax"));
  connect(syntaxAct_, SIGNAL(triggered()), SLOT(checkSyntax()));
  
  helpAct_ = new QAction(tr("&Show Help"), this);
  helpAct_->setObjectName("helpAct");
  helpAct_->setShortcut(tr("F1"));
  helpAct_->setStatusTip(tr("Show the help window"));
  connect(helpAct_, SIGNAL(triggered()), SLOT(help()));

  aboutAct_ = new QAction(tr("&About"), this);
  aboutAct_->setObjectName("aboutAct");
  aboutAct_->setStatusTip(tr("Show the application's About box"));
  connect(aboutAct_, SIGNAL(triggered()), SLOT(about()));

  aboutQtAct_ = new QAction(tr("About &Qt"), this);
  aboutQtAct_->setObjectName("aboutQtAct");
  aboutQtAct_->setStatusTip(tr("Show the Qt library's About box"));
  connect(aboutQtAct_, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
}

void MainForm::createMenus(void)
{
  QMenu * menu;
  QAction * sep;

  menu = menuBar()->addMenu(tr("&File"));
  menu->setObjectName("fileMenu");
  menu->addAction(newAct_);
  menu->addAction(openAct_);
  recentMenu_ = new RecentFilesMenu(tr("Open &Recent"), 5, menu);
  menu->addMenu(recentMenu_);
  connect(menu, SIGNAL(triggered(QAction *)), SLOT(openRecent(QAction *)));
  menu->addSeparator();
  menu->addAction(saveAct_);
  menu->addAction(saveAsAct_);
  menu->addAction(saveAllAct_);
  menu->addAction(reloadAct_);
  menu->addAction(reloadAllAct_);
  menu->addSeparator();
  menu->addAction(printAct_);
  menu->addAction(previewAct_);
  menu->addSeparator();
  menu->addAction(closeAct_);
  menu->addAction(closeAllAct_);
  menu->addSeparator();

  menu->addAction(exitAct_);

  menu = menuBar()->addMenu(tr("&Edit"));
  menu->setObjectName("editMenu");
  menu->addAction(undoAct_);
  menu->addAction(redoAct_);
  menu->addSeparator();
  menu->addAction(cutAct_);
  menu->addAction(copyAct_);
  menu->addAction(pasteAct_);
  menu->addSeparator();
  menu->addAction(overwriteAct_);
  menu->addSeparator();
  menu->addAction(findAct_);
  menu->addAction(findNextAct_);
  menu->addAction(findPrevAct_);
  menu->addAction(replaceAct_);

  menu = menuBar()->addMenu(tr("&View"));
  menu->setObjectName("viewMenu");

  QMenu * sub = menu->addMenu("&Toolbars");
  sub->setObjectName("toolbarsMenu");
  sub = menu->addMenu("&Docks");
  sub->setObjectName("docksMenu");
  menu->addSeparator();
  menu->addAction(wordWrapAct_);
  menu->addAction(numbersAct_);
  menu->addSeparator();
  menu->addAction(prefsAct_);

  menu = menuBar()->addMenu(tr("&Tools"));
  menu->setObjectName("toolsMenu");
  menu->addAction(syntaxAct_);

  sep = menu->addSeparator();
  sep->setObjectName("toolsSeparator");
  sep->setVisible(false);

  menu = menuBar()->addMenu(tr("Si&mulate"));
  menu->setObjectName("simMenu");
  menu->addAction(startAct_);
  menu->addAction(stepBackAct_);
  menu->addAction(stepForeAct_);
  menu->addAction(stepRandomAct_);
  menu->addAction(restartAct_);
  menu->addAction(stopAct_);
  sep = menu->addSeparator();
  sep->setObjectName("simSeparator1");
  menu->addAction(randomRunAct_);
  sep = menu->addSeparator();
  sep->setObjectName("simSeparator2");
  menu->addAction(importAct_);
  menu->addAction(exportAct_);

  menuBar()->addSeparator();

  menu = menuBar()->addMenu(tr("&Help"));
  menu->setObjectName("helpMenu");
  menu->addAction(helpAct_);
  menu->addSeparator();
  menu->addAction(aboutAct_);
  menu->addAction(aboutQtAct_);
}

void MainForm::createToolBars(void)
{
  QToolBar * toolbar;
  QAction * action;

  QMenu * menu = findChild<QMenu*>("toolbarsMenu");
  Q_ASSERT(menu);

  toolbar = addToolBar(tr("File"));
  toolbar->setObjectName("fileToolBar");
  toolbar->addAction(newAct_);
  toolbar->addAction(openAct_);
  toolbar->addAction(saveAct_);

  action = toolbar->toggleViewAction();
  action->setText(tr("&File"));
  action->setStatusTip(tr("Toggles the file toolbar"));
  menu->addAction(action);

  toolbar = addToolBar(tr("Edit"));
  toolbar->setObjectName("editToolBar");
  toolbar->addAction(undoAct_);
  toolbar->addAction(redoAct_);
  toolbar->addAction(cutAct_);
  toolbar->addAction(copyAct_);
  toolbar->addAction(pasteAct_);

  action = toolbar->toggleViewAction();
  action->setText(tr("&Edit"));
  action->setStatusTip(tr("Toggles the edit toolbar"));
  menu->addAction(action);

  toolbar = addToolBar(tr("Simulate"));
  toolbar->setObjectName("simToolBar");
  toolbar->addAction(startAct_);
  toolbar->addAction(stepBackAct_);
  toolbar->addAction(stepForeAct_);
  toolbar->addAction(stepRandomAct_);
  toolbar->addAction(restartAct_);
  toolbar->addAction(stopAct_);

  action = toolbar->toggleViewAction();
  action->setText(tr("&Simulation"));
  action->setStatusTip(tr("Toggles the simulator toolbar"));
  menu->addAction(action);
}

void MainForm::createStatusBar(void)
{
  statusLabel_ = new QLabel(tr("Line: %1 Col: %2").arg("1", "1"), statusBar());
  statusBar()->addPermanentWidget(statusLabel_);

  statusBar()->showMessage(tr("Ready"));
}

void MainForm::readSettings(void)
{
  QByteArray geometry = sSettings().value("geometry", QByteArray()).toByteArray();

  if (!geometry.isEmpty()) {
    restoreGeometry(geometry);
  } else {
    resize(500, 400);
    move(200, 150);
  }

  layman_->readSettings();

  // read session
  QStringList files = sSettings().value("session", QStringList()).toStringList();
  foreach(QString file, files) {
    openFile(file);
  }

  int doc = sSettings().value("sessionDoc", 0).toInt();
  pageArea_->setCurrentIndex(doc);
}

void MainForm::writeSettings(void)
{
  sSettings().setValue("geometry", saveGeometry());

  layman_->writeSettings();

  // save session
  QStringList files;

  for (int i = 0; i < pageArea_->count(); ++i) {
    SourceEditor * editor = qobject_cast<SourceEditor*>(pageArea_->widget(i));

    if (editor) {
      QUrl url(editor->document()->metaInformation(QTextDocument::DocumentUrl));
      files.append(url.path());
    }
  }

  sSettings().setValue("session", files);

  sSettings().setValue("sessionDoc", pageArea_->currentIndex());
}

EditorBuilder * MainForm::getBuilder(const QString & suffix)
{
  EditorBuilder * res = NULL;

  foreach(BuilderList::value_type itr, docBuilders_) {
    if (itr.first.first == suffix) {
      res = itr.second;
      break;
    }
  }

  return res;
}

bool MainForm::checkSyntaxErrors(Simulator * sim)
{
  Q_ASSERT(sim);
  
  SourceEditor * editor;
  QUrl url;
  
  SyntaxErrorList err_list = sim->errors();
  
  emit message(tr("=== Syntax check ==="));
  
  if(!err_list.isEmpty()) {
    for (int i = 0; i < pageArea_->count(); ++i) {
      editor = qobject_cast<SourceEditor*>(pageArea_->widget(i));
      url = editor->document()->metaInformation(QTextDocument::DocumentUrl);
      
      foreach(SyntaxError err, err_list) {
        if(err.file != url.path())
          continue;
        
        if(!err.block.isNull())
          editor->highlightBlock(err.block, "salmon");
      
        emit message(QString("%1: %2").arg(QFileInfo(err.file).fileName(), err.message));
      }
    }
  } else {
    emit message(tr("Ok"));
  }
  return !err_list.empty();
}

void MainForm::open()
{
  QStringList filters;

  foreach(BuilderList::value_type itr, docBuilders_) {
    filters << itr.first.second;
  }

  filters << tr("All files (*)");

  QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"),
                     "", filters.join(";;"));

  openFile(fileName);
}

void MainForm::openRecent(QAction * action)
{
  Q_ASSERT(action);

  // if action does not contain a filename, toString() will return empty string
  openFile(action->data().toString());
}

bool MainForm::save(SourceEditor * editor)
{
  if (!editor)
    editor = qobject_cast<SourceEditor*>(pageArea_->currentWidget());

  if (!editor)
    return true;

  QString path = QUrl(editor->document()->metaInformation(QTextDocument::DocumentUrl)).path();

  if (path.isEmpty())
    return saveAs(editor);

  return editor->saveFile(path);
}

bool MainForm::saveAs(SourceEditor * editor)
{
  if (!editor)
    editor = qobject_cast<SourceEditor*>(pageArea_->currentWidget());

  if (!editor)
    return true;

  QStringList filters;
  QString defFilter;

  QUrl old_url = editor->document()->metaInformation(QTextDocument::DocumentUrl);
  
  foreach(BuilderList::value_type itr, docBuilders_) {
    filters << itr.first.second;
    
    if(itr.first.first == QFileInfo(old_url.path()).suffix())
      defFilter = itr.first.second;
  }

  filters << tr("All files (*)");
  
  QFileDialog dlg(this, tr("Save File"), "", filters.join(";;"));
  dlg.setAcceptMode(QFileDialog::AcceptSave);
  
  // select current suffix as default filter
  if(!defFilter.isEmpty())
    dlg.selectFilter(defFilter);
  
  // open the save dialog
  if(!dlg.exec())
    return false;
  
  QString fileName = dlg.selectedFiles().at(0);
  
  // append suffix if none provided
  if(QFileInfo(fileName).suffix().isEmpty()) {
    for(int i = 0; i < docBuilders_.size(); ++i) {
      if(docBuilders_[i].first.second ==  dlg.selectedFilter()) {
        fileName.append(".");
        fileName.append(docBuilders_[i].first.first);
        break;
      }
    }
  }

  QUrl url;
  QFileInfo finfo(fileName);
  
  qDebug("%s",finfo.absoluteFilePath().toAscii().data());
  
  // check for new duplicities
  for(int i = 0; i < pageArea_->count();) {
    SourceEditor * ed2 = qobject_cast<SourceEditor*>(pageArea_->widget(i));
    Q_ASSERT(ed2);
    
    if(ed2 == editor) {
      ++i;
      continue;
    }
    
    url = ed2->document()->metaInformation(QTextDocument::DocumentUrl);
    qDebug("%d %s",i,url.path().toAscii().data());
    // file is already opened
    if(finfo.absoluteFilePath() == url.path()) {
      if(!closeFile(ed2))
        return false;
    } else {
      ++i;
    }
  }
  
  if(!editor->saveFile(fileName))
     return false;
  
  // update recent files
  recentMenu_->addFile(fileName);
  
  url.setScheme(finfo.suffix());
  url.setPath(finfo.absoluteFilePath());
  
  editor->document()->setMetaInformation(QTextDocument::DocumentTitle, finfo.fileName());
  editor->document()->setMetaInformation(QTextDocument::DocumentUrl, url.toString());

  // do we need to switch highlighters?
  if (old_url.scheme() != finfo.suffix()) {
    // find the original and the new builder
    EditorBuilder * builder = getBuilder(old_url.scheme());

    if (builder)
      builder->uninstall(editor);

    if ((builder = getBuilder(finfo.suffix())))
      builder->install(editor);
  }

  // update the watcher
  if (!old_url.path().isEmpty())
    watcher_->removePath(old_url.path());

  watcher_->addPath(finfo.absoluteFilePath());
  pageArea_->setTabText(pageArea_->currentIndex(), editor->documentTitle());
  
  emit editorChanged(editor);
  
  return true;
}

void MainForm::saveAll(void)
{
  for (int i = 0; i < pageArea_->count(); ++i) {
    SourceEditor * editor = qobject_cast<SourceEditor*>(pageArea_->widget(i));
    Q_ASSERT(editor);

    if (editor->document()->isModified())
      save(editor);
  }
}

void MainForm::reload(SourceEditor * editor)
{
  if (!editor)
    editor = qobject_cast<SourceEditor*>(pageArea_->currentWidget());

  Q_ASSERT(editor);

  QUrl url(editor->document()->metaInformation(QTextDocument::DocumentUrl));

  if (editor->document()->isModified() && !url.path().isEmpty())
    editor->loadFile(url.path());
}

void MainForm::reloadAll()
{
  for (int i = 0; i < pageArea_->count(); ++i) {
    SourceEditor * editor = qobject_cast<SourceEditor*>(pageArea_->widget(i));
    Q_ASSERT(editor);

    reload(editor);
  }
}

void MainForm::print()
{
  SourceEditor * editor = qobject_cast<SourceEditor*>(pageArea_->currentWidget());
  Q_ASSERT(editor);

  QPrinter printer;

  QPrintDialog * dialog = new QPrintDialog(&printer, this);
  dialog->setWindowTitle(tr("Print Document"));

  if (editor->textCursor().hasSelection())
    dialog->addEnabledOption(QAbstractPrintDialog::PrintSelection);

  if (dialog->exec() != QDialog::Accepted)
    return;

  editor->print(&printer);
}

void MainForm::preview()
{
  SourceEditor * editor = qobject_cast<SourceEditor*>(pageArea_->currentWidget());
  Q_ASSERT(editor);

  QPrintPreviewDialog dlg(this);

  connect(&dlg, SIGNAL(paintRequested(QPrinter*)), SLOT(onPrintRequested(QPrinter*)));

  dlg.exec();
}

bool MainForm::closeFile(SourceEditor * editor)
{
  if (!editor)
    editor = qobject_cast<SourceEditor*>(pageArea_->currentWidget());

  if (!editor)
    return false;

  // check if we are not closing the last file of the simulation
  QStringList files;

  if (simProxy_)
    files = simProxy_->simulator()->files();

  int cnt = 0;

  for (int i = 0; i < pageArea_->count(); ++i) {
    SourceEditor * ed = qobject_cast<SourceEditor*>(pageArea_->widget(i));
    QUrl url(ed->document()->metaInformation(QTextDocument::DocumentUrl));

    if (files.contains(url.path()))
      ++cnt;
  }

  QUrl url(editor->document()->metaInformation(QTextDocument::DocumentUrl));

  if (cnt == 1 && files.contains(url.path()) && !maybeStop())
    return false;

  if (maybeSave(editor)) {
    // remove from watcher
    if (!url.path().isEmpty())
      watcher_->removePath(url.path());

    pageArea_->removeTab(pageArea_->indexOf(editor));

    editor->deleteLater();
    return true;
  }
  return false;
}

void MainForm::closeAll(void)
{
  if (maybeStop() && maybeSaveAll()) {
    watcher_->removePaths(watcher_->files());

    while (pageArea_->count()) {
      pageArea_->removeTab(pageArea_->currentIndex());
    }
  }
}

//! Displays the search dialog.
void MainForm::showFindDialog()
{
  QString hint;

  SourceEditor * editor = qobject_cast<SourceEditor*>(pageArea_->currentWidget());
  Q_ASSERT(editor);

  if (editor->textCursor().hasSelection()) {
    hint = editor->textCursor().selectedText();
  } else {
    QTextCursor cur = editor->textCursor();
    cur.select(QTextCursor::WordUnderCursor);
    hint = cur.selectedText();
  }

  search_->initialize(hint, false);

  search_->show();
  search_->activateWindow();
  search_->raise();
}

//! Displays the search and replace dialog.
void MainForm::showReplaceDialog()
{
  QString hint;

  SourceEditor * editor = qobject_cast<SourceEditor*>(pageArea_->currentWidget());
  Q_ASSERT(editor);

  if (editor->textCursor().hasSelection()) {
    hint = editor->textCursor().selectedText();
  } else {
    QTextCursor cur = editor->textCursor();
  }

  search_->initialize(hint, true);

  search_->show();
  search_->activateWindow();
  search_->raise();
}

//! Shows the references dialog.
void MainForm::showPreferences(void)
{
  preferences_->initialize();

  if (preferences_->exec())
    emit settingsChanged();
}

void MainForm::help(void)
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
    
    helpProc_->start(QString("%1 -enableRemoteControl -collectionFile %2/divine-ide.qhc").arg(ASSISTANT_BIN, helpdir),
                     QIODevice::WriteOnly);
    helpProc_->waitForStarted();

    if (helpProc_->state() != QProcess::Running) {
      QMessageBox::warning(this, tr("DiVinE IDE"), tr("Error launching help viewer!"));
    }
  }
}

//! Displays the application's about box
void MainForm::about(void)
{
  QMessageBox::about(this, tr("About DiVinE IDE"),
                     tr("<b>DiVinE IDE</b> is a development environment for DiViNE LTL model"
                        "checking system.<br><br>Project web page: "
                        "<a href=\"http://divine.fi.muni.cz\">http://divine.fi.muni.cz</a>"));
}

void MainForm::randomStep(void)
{
  const Simulator * simulator = simProxy_ ? simProxy_->simulator() : NULL;

  if (!simulator || !simulator->transitionCount())
    return;

  int rnd = qrand() % simulator->transitionCount();

  simProxy_->step(rnd, this);
}

void MainForm::randomRun(void)
{
  const Simulator * simulator = simProxy_ ? simProxy_->simulator() : NULL;

  if (!simulator || !simulator->transitionCount())
    return;
  
  // clear highlighting
  for (int i = 0; i < pageArea_->count(); ++i) {
    SourceEditor * editor = qobject_cast<SourceEditor*>(pageArea_->widget(i));
    editor->resetHighlighting();
  }
  
  sSettings().beginGroup("Simulator");
  
  int steps = sSettings().value("steps", defSimulatorSteps).toInt();
  int delay = sSettings().value("delay", defSimulatorDelay).toInt();
  
  sSettings().endGroup();
  
  bool ok;
  steps = QInputDialog::getInteger(this, tr("Random run"),
    tr("Select number of steps to run:"), steps, 1, 800, 1, &ok);

  if (!ok)
    return;
    
  if(delay == 0) {
    for(int i = 0; i < steps && simulator->transitionCount() > 0; ++i)
      randomStep();
  } else {
    simProxy_->lock(this);
    
    for (int i = 0; i < pageArea_->count(); ++i) {
      SourceEditor * editor = qobject_cast<SourceEditor*>(pageArea_->widget(i));
      editor->autoHighlight(NULL);
    }
    
    steps_ = steps;
    randomTimer_->setInterval(delay);
    connect(randomTimer_, SIGNAL(timeout()), SLOT(randomTimeout()));
    randomTimer_->start();
  }
}

void MainForm::randomTimeout(void)
{
  Q_ASSERT(simProxy_ && simProxy_->simulator());
  Q_ASSERT(simProxy_->simulator()->transitionCount() > 0);

  int rnd = qrand() % simProxy_->simulator()->transitionCount();
  
  Simulator::TransitionList trans = simProxy_->simulator()->transitionSource(rnd);
  SourceEditor * editor;
  
  // highlight the transition
  for (int i = 0; i < pageArea_->count(); ++i) {
    editor = qobject_cast<SourceEditor*>(pageArea_->widget(i));
    editor->resetHighlighting();
    
    foreach(Simulator::TransitionPair itr, trans) {
      QString path = QUrl(editor->document()->metaInformation(QTextDocument::DocumentUrl)).path();
      if (itr.first != path)
          continue;

      editor->highlightBlock(itr.second, QColor("burlywood"));
    }
  }
  
  simProxy_->step(rnd, this);
  
  if(--steps_ <= 0 || simProxy_->simulator()->transitionCount() == 0) {
    disconnect(randomTimer_, SIGNAL(timeout()), this, SLOT(randomTimeout()));
    randomTimer_->stop();
    
    simProxy_->release(this);

    // re-enable auto highlighting
    for (int i = 0; i < pageArea_->count(); ++i) {
      editor = qobject_cast<SourceEditor*>(pageArea_->widget(i));
      editor->resetHighlighting();
      editor->autoHighlight(simProxy_);
    }
  }
}

void MainForm::importRun(void)
{
  const QString fileName = QFileDialog::getOpenFileName(
    this, tr("Open file"), "", tr("All Files (*)"));

  if (fileName.isEmpty())
    return;

  startSimulation();

  const Simulator * simulator = simProxy_ ? simProxy_->simulator() : NULL;

  if (!simulator)
    return;

  const_cast<Simulator*>(simulator)->loadTrail(fileName);
}

void MainForm::exportRun(void)
{
  const Simulator * simulator = simProxy_ ? simProxy_->simulator() : NULL;

  if (!simulator)
    return;

  const QString fileName = QFileDialog::getSaveFileName(
    this, tr("Save file"), "", tr("All Files (*)"));

  if (!fileName.isEmpty())
    simulator->saveTrail(fileName);
}

void MainForm::checkSyntax()
{
  Q_ASSERT(pageArea_->currentWidget());

  SourceEditor * editor = qobject_cast<SourceEditor*>(pageArea_->currentWidget());
  QUrl url(editor->document()->metaInformation(QTextDocument::DocumentUrl));

  // if current page is untitled -> force save as
  if(url.path().isEmpty()) {
    if(!save(editor))
      return;
  }
  
  // auto save
  for(int i = 0; i < pageArea_->count(); ++i) {
    editor = qobject_cast<SourceEditor*>(pageArea_->widget(i));
    url = editor->document()->metaInformation(QTextDocument::DocumentUrl);
  
    if(editor->document()->isModified() && !url.path().isEmpty())
      save(editor);
  }
  
  editor = qobject_cast<SourceEditor*>(pageArea_->currentWidget());
  url = editor->document()->metaInformation(QTextDocument::DocumentUrl);
  
  SimulatorFactory * factory = simLoaders_[url.scheme()];
  Q_ASSERT(factory);

  Simulator * simulator = factory->load(url.path(), this);

  if (!simulator)
    return;
  
  // clear highlights
  for (int i = 0; i < pageArea_->count(); ++i) {
    editor = qobject_cast<SourceEditor*>(pageArea_->widget(i));
    editor->resetHighlighting();
  }
  
  // check for errors
  if(checkSyntaxErrors(simulator)) {
    QMessageBox::warning(NULL, tr("Syntax Check"),
                         tr("Source contains errors!"));
  } else {
    QMessageBox::information(NULL, tr("Syntax Check"),
                             tr("No errors found."));
  }

  delete simulator;
}

void MainForm::findNext(void)
{
  SourceEditor * editor = qobject_cast<SourceEditor*>(pageArea_->currentWidget());

  if (!editor)
    return;

  const QTextDocument::FindFlags flags = search_->flags();
  
  QTextCursor cur = editor->document()->find(
                      search_->pattern(), editor->textCursor(), flags);

  if (cur.isNull()) {
    cur = editor->document()->find(search_->pattern(), 0, flags);

    if (cur.isNull())
      statusBar()->showMessage(tr("No match"));
    else
      statusBar()->showMessage(tr("Passed the end of file"));
  }

  if (!cur.isNull())
    editor->setTextCursor(cur);
}

void MainForm::findPrevious(void)
{
  SourceEditor * editor = qobject_cast<SourceEditor*>(pageArea_->currentWidget());

  if (!editor)
    return;

  const QTextDocument::FindFlags flags = search_->flags() ^ QTextDocument::FindBackward;

  QTextCursor cur = editor->document()->find(
                      search_->pattern(), editor->textCursor(), flags);

  if (cur.isNull()) {
    cur = editor->textCursor();
    cur.movePosition(QTextCursor::End);

    cur = editor->document()->find(search_->pattern(), cur, flags);

    if (cur.isNull())
      statusBar()->showMessage(tr("No match"));
    else
      statusBar()->showMessage(tr("Passed the end of file"));
  }

  if (!cur.isNull())
    editor->setTextCursor(cur);
}

void MainForm::replace(void)
{
  SourceEditor * editor = qobject_cast<SourceEditor*>(pageArea_->currentWidget());

  if (!editor)
    return;

  if (editor->textCursor().hasSelection())
    editor->textCursor().insertText(search_->sample());

  findNext();
}

void MainForm::replaceAll(void)
{
  SourceEditor * editor = qobject_cast<SourceEditor*>(pageArea_->currentWidget());

  if (!editor)
    return;

  const QTextDocument::FindFlags flags = search_->flags() & ~QTextDocument::FindBackward;

  QTextCursor cur = editor->textCursor();

  cur.movePosition(QTextCursor::Start);

  do {
    cur = editor->document()->find(search_->pattern(), cur, flags);
    cur.insertText(search_->sample());
  } while (!cur.isNull());
}

void MainForm::onPrintRequested(QPrinter * printer)
{
  SourceEditor * editor = qobject_cast<SourceEditor*>(pageArea_->currentWidget());

  if (!editor)
    return;

  editor->print(printer);
}

void MainForm::onSimulatorStarted(void)
{
  // stop random runner
  if(randomTimer_->isActive()) {
    disconnect(randomTimer_, SIGNAL(timeout()), this, SLOT(randomTimeout()));
    randomTimer_->stop();
    simProxy_->release(this);
    
    // clear highlighting
    for (int i = 0; i < pageArea_->count(); ++i) {
      SourceEditor * editor = qobject_cast<SourceEditor*>(pageArea_->widget(i));
      editor->resetHighlighting();
    }
  }
  
  updateSimulator();
}

void MainForm::onSimulatorStopped(void)
{
  if (!simProxy_)
    return;

  // stop random runner
  if(randomTimer_->isActive()) {
    disconnect(randomTimer_, SIGNAL(timeout()), this, SLOT(randomTimeout()));
    randomTimer_->stop();
    simProxy_->release(this);
    
    // clear highlighting
    for (int i = 0; i < pageArea_->count(); ++i) {
      SourceEditor * editor = qobject_cast<SourceEditor*>(pageArea_->widget(i));
      editor->resetHighlighting();
    }
  }
  
  layman_->switchLayout("edit");

  // disconnect actions
  stopAct_->disconnect(simProxy_);
  restartAct_->disconnect(simProxy_);
  stepBackAct_->disconnect(simProxy_);
  stepForeAct_->disconnect(simProxy_);

  disconnect(simProxy_, SIGNAL(started()), this, SLOT(onSimulatorStarted()));
  disconnect(simProxy_, SIGNAL(stateChanged()), this, SLOT(updateSimulator()));
  disconnect(simProxy_, SIGNAL(stopped()), this, SLOT(onSimulatorStopped()));
  disconnect(simProxy_, SIGNAL(locked(bool)), this, SLOT(updateSimulator()));

  // disable read-only modes
  for (int i = 0; i < pageArea_->count(); ++i) {
    SourceEditor * editor = qobject_cast<SourceEditor*>(pageArea_->widget(i));
    Q_ASSERT(editor);

    editor->setReadOnly(false);
    editor->autoHighlight(NULL);
  }

  delete simProxy_;
  simProxy_ = NULL;

  emit message(tr("=== Simulation stopped ==="));
  emit simulatorChanged(NULL);

  updateWorkspace();
  updateSimulator();
}

void MainForm::onOverwriteToggled(bool checked)
{
  SourceEditor * editor = qobject_cast<SourceEditor*>(pageArea_->currentWidget());

  if (!editor)
    return;

  editor->setOverwriteMode(checked);
}

void MainForm::onWordWrapToggled(bool checked)
{
  SourceEditor * editor = qobject_cast<SourceEditor*>(pageArea_->currentWidget());

  if (!editor)
    return;

  editor->setLineWrapMode(checked ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
}

void MainForm::onLineNumbersToggled(bool checked)
{
  SourceEditor * editor = qobject_cast<SourceEditor*>(pageArea_->currentWidget());

  if (!editor)
    return;

  editor->setShowLineNumbers(checked);
}

void MainForm::onTabCurrentChanged(int index)
{
  // disconnect the previous editor
  undoAct_->disconnect();
  redoAct_->disconnect();
  cutAct_->disconnect();
  copyAct_->disconnect();
  pasteAct_->disconnect();

  SourceEditor * editor = qobject_cast<SourceEditor*>(pageArea_->currentWidget());

  if (editor) {
    // connect the new editor
    connect(undoAct_, SIGNAL(triggered(bool)), editor, SLOT(undo()));
    connect(redoAct_, SIGNAL(triggered(bool)), editor, SLOT(redo()));
    connect(cutAct_, SIGNAL(triggered(bool)), editor, SLOT(cut()));
    connect(copyAct_, SIGNAL(triggered(bool)), editor, SLOT(copy()));
    connect(pasteAct_, SIGNAL(triggered(bool)), editor, SLOT(paste()));

    overwriteAct_->setChecked(editor->overwriteMode());
    wordWrapAct_->setChecked(editor->lineWrapMode() != QPlainTextEdit::NoWrap);
    numbersAct_->setChecked(editor->showLineNumbers());
  }

  updateWindowTitle();

  updateWorkspace();
  updateSimulator();
  updateStatusLabel();

  emit editorChanged(editor);
}

void MainForm::onTabCloseRequested(int index)
{
  SourceEditor * editor = qobject_cast<SourceEditor*>(pageArea_->widget(index));

  if (editor)
    closeFile(editor);
}

void MainForm::onFileChanged(const QString & fileName)
{
  SourceEditor * edit = editor(fileName);

  if (!edit)
    return;

  QFile file(fileName);

  if (!file.open(QFile::ReadOnly | QFile::Text))
    return;

  QTextStream in(&file);

  if (edit->toPlainText() != in.readAll())
    edit->document()->setModified();
}
