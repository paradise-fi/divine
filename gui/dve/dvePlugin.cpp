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

#include <divine/dve/parse.h>

#include <QtPlugin>
#include <QCompleter>
#include <QUrl>
#include <QMenu>
#include <QAction>
#include <QProgressBar>
#include <QPushButton>
#include <QMessageBox>
#include <QInputDialog>

#include "dvePlugin.h"
#include "dveDocument.h"
#include "dveHighlighter.h"
#include "dvePreferences.h"
#include "dveSimulator.h"
#include "ltlDocument.h"
#include "ltlHighlighter.h"
#include "ltlPreferences.h"

#include "combine.h"
#include "combineDialog.h"

#include "settings.h"
#include "mainForm.h"
#include "preferencesDialog.h"
#include "pluginManager.h"
#include "divineTools.h"
#include "simulationProxy.h"

#include "textDocument.h"
#include "textEditor.h"
#include "textEditorHandlers.h"

namespace divine {
namespace gui {

namespace {
  QVector<int> extract(const QString & trail)
  {
    QVector<int> res;
    QStringList spl = trail.split(',');

    foreach(QString str, spl) {
      // trail in meta.result is numbered from 1
      res.append(str.toInt() - 1);
    }
    return res;
  }
}

//
// Dve editor factory
//
QString DveDocumentFactory::name() const
{
  return QObject::tr("DVE file");
}

QString DveDocumentFactory::suffix() const
{
  return "dve";
}

QIcon DveDocumentFactory::icon() const
{
  return MainForm::loadIcon(":/icons/filetypes/generic");
}

QString DveDocumentFactory::filter() const
{
  return QObject::tr("DVE files");
}

AbstractDocument * DveDocumentFactory::create(MainForm * root) const
{
  DveDocument * document = new DveDocument(root);
  TextEditor * editor = qobject_cast<TextEditor*>(document->editor(document->mainView()));
  Q_ASSERT(editor);

  // completer
  QStringList words;
  words << "process" << "state" << "init" << "trans" << "channel" << "sync"
        << "system" << "assert" << "guard" << "effect" << "commit" << "const"
        << "async" << "accept" << "property"
        << "byte" << "int" << "true" << "false"
        << "imply" << "and" << "or" << "not";
  words.sort();

  SimpleCompleter * completer = new SimpleCompleter(words, editor);
  editor->addKeyEventHandler(completer);

  // basic handlers
  SimpleIndenter * indenter = new SimpleIndenter(editor);
  connect(root, SIGNAL(settingsChanged()), indenter, SLOT(readSettings()));
  editor->addKeyEventHandler(indenter);

  SimpleTabConverter * converter = new SimpleTabConverter(editor);
  connect(root, SIGNAL(settingsChanged()), converter, SLOT(readSettings()));
  editor->addKeyEventHandler(converter);

  // highlighter
  DveHighlighter * highlight = new DveHighlighter(false, editor->textDocument());
  connect(root, SIGNAL(settingsChanged()), highlight, SLOT(readSettings()));

  return document;
}

//
// Dve editor factory
//
QString MDveDocumentFactory::name() const
{
  return QObject::tr("MDVE file");
}

QString MDveDocumentFactory::suffix() const
{
  return "mdve";
}

QIcon MDveDocumentFactory::icon() const
{
  return MainForm::loadIcon(":/icons/filetypes/generic");
}

QString MDveDocumentFactory::filter() const
{
  return QObject::tr("MDVE files");
}

AbstractDocument * MDveDocumentFactory::create(MainForm * root) const
{
  MDveDocument * document = new MDveDocument(root);
  TextEditor * editor = qobject_cast<TextEditor*>(document->editor(document->mainView()));
  Q_ASSERT(editor);

  // completer
  QStringList words;
  words << "forloop" << "bool" << "stack" << "queue" << "full" << "empty"
        << "push" << "pop" << "top" << "pop_front" << "buffer_channel"
        << "async_channel" << "loosy_channel" << "bounded_loosy_channel"
        << "default" << "define"
        << "process" << "state" << "init" << "trans" << "channel" << "sync"
        << "system" << "assert" << "guard" << "effect" << "commit" << "const"
        << "async" << "accept" << "property"
        << "byte" << "int" << "true" << "false"
        << "imply" << "and" << "or" << "not";
  words.sort();

  SimpleCompleter * completer = new SimpleCompleter(words, editor);
  editor->addKeyEventHandler(completer);

  // basic handlers
  SimpleIndenter * indenter = new SimpleIndenter(editor);
  connect(root, SIGNAL(settingsChanged()), indenter, SLOT(readSettings()));
  editor->addKeyEventHandler(indenter);

  SimpleTabConverter * converter = new SimpleTabConverter(editor);
  connect(root, SIGNAL(settingsChanged()), converter, SLOT(readSettings()));
  editor->addKeyEventHandler(converter);

  // highlighter
  DveHighlighter * highlight = new DveHighlighter(true, editor->textDocument());
  connect(root, SIGNAL(settingsChanged()), highlight, SLOT(readSettings()));

  return document;
}

//
// Ltl editor factory
//
QString LtlDocumentFactory::name() const
{
  return QObject::tr("LTL file");
}

QString LtlDocumentFactory::suffix() const
{
  return "ltl";
}

QIcon LtlDocumentFactory::icon() const
{
  return MainForm::loadIcon(":/icons/filetypes/generic");
}

QString LtlDocumentFactory::filter() const
{
  return QObject::tr("LTL files");
}

AbstractDocument * LtlDocumentFactory::create(MainForm * root) const
{
  LtlDocument * document = new LtlDocument(root);
  TextEditor * editor = qobject_cast<TextEditor*>(document->editor(document->mainView()));
  Q_ASSERT(editor);

  // basic handlers
  SimpleIndenter * indenter = new SimpleIndenter(editor);
  connect(root, SIGNAL(settingsChanged()), indenter, SLOT(readSettings()));
  editor->addKeyEventHandler(indenter);

  SimpleTabConverter * converter = new SimpleTabConverter(editor);
  connect(root, SIGNAL(settingsChanged()), converter, SLOT(readSettings()));
  editor->addKeyEventHandler(converter);

  LtlHighlighter * highlight = new LtlHighlighter(editor->textDocument());

  connect(root, SIGNAL(settingsChanged()), highlight, SLOT(readSettings()));

  return document;
}

//
// Dve simulator factory
//
QString DveSimulatorFactory::suffix() const
{
  return "dve";
}

AbstractSimulator * DveSimulatorFactory::create(MainForm * root) const
{
  DveSimulator * sim = new DveSimulator(root);
  connect(sim, SIGNAL(message(QString)), root, SIGNAL(message(QString)));
  return sim;
}

//
// Dve plugin
//
DvePlugin::DvePlugin()
{
}

void DvePlugin::install(MainForm * root)
{
  Q_ASSERT(root);
  root_ = root;

  runner_ = new DivineRunner(this);
  lock_ = new DivineLock(root_, runner_, this);

  DivineOutput * output = new DivineOutput(this);
  Output::_output = output;

  connect(output, SIGNAL(message(QString)), this, SIGNAL(message(QString)));
  connect(this, SIGNAL(message(QString)), root_, SIGNAL(message(QString)));

  // populate the divine menu
  syntaxAct_ = new QAction(tr("Check &syntax"), this);
  syntaxAct_->setObjectName("dve.action.dve.syntax");
  syntaxAct_->setStatusTip(tr("Check syntax"));
  connect(syntaxAct_, SIGNAL(triggered()), SLOT(onSyntaxTriggered()));

  preprocessorAct_ = new QAction(tr("&Preprocessor..."), this);
  preprocessorAct_->setObjectName("dve.action.dve.preprocessor");
  preprocessorAct_->setStatusTip(tr("Preprocess MDVE file"));
  connect(preprocessorAct_, SIGNAL(triggered()), SLOT(onPreprocessorTriggered()));

  combineAct_ = new QAction(tr("&Combine..."), this);
  combineAct_->setObjectName("dve.action.dve.combine");
  combineAct_->setStatusTip(tr("Apply LTL formula to DVE file"));
  connect(combineAct_, SIGNAL(triggered()), SLOT(onCombineTriggered()));

  QMenu * menu = root->findChild<QMenu*>("main.menu.tools");
  dveSep_ = menu->insertSeparator(menu->actions().first());
  menu->insertAction(dveSep_, syntaxAct_);
  menu->insertAction(dveSep_, preprocessorAct_);
  menu->insertAction(dveSep_, combineAct_);

  // register tracking hooks
  connect(root, SIGNAL(editorActivated(AbstractEditor*)), SLOT(updateActions()));
  connect(root, SIGNAL(editorDeactivated()), SLOT(onEditorDeactivated()));
  connect(root, SIGNAL(lockChanged()), SLOT(updateActions()));

  // preferences
  QWidget * page = new DvePreferences();
  root->preferences()->addWidget(QObject::tr("Syntax"), QObject::tr("DVE"), page);

  page = new LtlPreferences();
  root->preferences()->addWidget(QObject::tr("Syntax"), QObject::tr("LTL"), page);

  // editors
  root->plugins()->registerDocument(new DveDocumentFactory(this));
  root->plugins()->registerDocument(new MDveDocumentFactory(this));
  root->plugins()->registerDocument(new LtlDocumentFactory(this));

  // simulator
  root->plugins()->registerSimulator(new DveSimulatorFactory(this));
}

void DvePlugin::updateActions()
{
  AbstractEditor * editor = root_->activeEditor();
  if(!editor)
    return;

  bool isDve = qobject_cast<DveDocument*>(editor->document()) != NULL;
  bool isMdve = qobject_cast<MDveDocument*>(editor->document()) != NULL;

  // not out type of document
  if(!isDve && !isMdve)
    return;

  bool locked = root_->isLocked();

  syntaxAct_->setVisible(true);
  preprocessorAct_->setVisible(true);
  combineAct_->setVisible(true);
  dveSep_->setVisible(true);

  syntaxAct_->setEnabled(isDve && !locked);
  preprocessorAct_->setEnabled(isMdve && !locked);
  combineAct_->setEnabled(isDve && !locked);

  if(isDve) {
    QAction * metricsAct = root_->findChild<QAction*>("main.action.divine.metrics");
    QAction * reachAct = root_->findChild<QAction*>("main.action.divine.reachability");
    QAction * searchAct = root_->findChild<QAction*>("main.action.divine.search");
    QAction * verifyAct = root_->findChild<QAction*>("main.action.divine.verify");

    metricsAct->setEnabled(!locked);
    reachAct->setEnabled(!locked);
// TODO: implement search?
//     searchAct->setEnabled(!locked);
    verifyAct->setEnabled(!locked);

    connect(metricsAct, SIGNAL(triggered()), SLOT(onMetricsTriggered()));
    connect(reachAct, SIGNAL(triggered()), SLOT(onReachabilityTriggered()));
    connect(searchAct, SIGNAL(triggered()), SLOT(onSearchTriggered()));
    connect(verifyAct, SIGNAL(triggered()), SLOT(onVerifyTriggered()));
  }
}

void DvePlugin::onEditorDeactivated()
{
  syntaxAct_->setVisible(false);
  preprocessorAct_->setVisible(false);
  combineAct_->setVisible(false);
  dveSep_->setVisible(false);

  QAction * metricsAct = root_->findChild<QAction*>("main.action.divine.metrics");
  QAction * reachAct = root_->findChild<QAction*>("main.action.divine.reachability");
  QAction * searchAct = root_->findChild<QAction*>("main.action.divine.search");
  QAction * verifyAct = root_->findChild<QAction*>("main.action.divine.verify");

  metricsAct->setEnabled(false);
  reachAct->setEnabled(false);
  searchAct->setEnabled(false);
  verifyAct->setEnabled(false);

  disconnect(metricsAct, SIGNAL(triggered()), this, SLOT(onMetricsTriggered()));
  disconnect(reachAct, SIGNAL(triggered()), this, SLOT(onReachabilityTriggered()));
  disconnect(searchAct, SIGNAL(triggered()), this, SLOT(onSearchTriggered()));
  disconnect(verifyAct, SIGNAL(triggered()), this, SLOT(onVerifyTriggered()));
}

void DvePlugin::onSyntaxTriggered()
{
  Q_ASSERT(root_->activeEditor());
  QByteArray input = qobject_cast<TextEditor*>(
    root_->activeEditor())->textDocument()->toPlainText().toAscii();

  std::stringstream ss(input.data());

  QString viewName = root_->activeEditor()->viewName();

  // load the dve as it is in the editor
  dve::IOStream stream(ss);
  dve::Lexer<dve::IOStream> lexer(stream);
  dve::Parser::Context ctx(lexer);

  try {
    dve::parse::System ast(ctx);
  } catch (...) {
    std::stringstream what;
    ctx.errors(what);

    emit message(tr("%1: Syntax check FAILED\n%2").arg(viewName, what.str().c_str()));
    return;
  }
  emit message(tr("%1: Syntax check OK").arg(viewName));
}

void DvePlugin::onPreprocessorTriggered()
{
  Q_ASSERT(root_->activeEditor());

  Settings s(root_->activeEditor()->document());

  QString defs = s.value("preprocessor", "").toString();
  bool ok;

  defs = QInputDialog::getText(
    root_, tr("Preprocessor"), tr("Enter a list of space separated definitions:"),
    QLineEdit::Normal, defs, &ok);

  if(!ok)
    return;

  s.setValue("preprocessor", defs);

  QByteArray input = qobject_cast<TextEditor*>(
    root_->activeEditor())->textDocument()->toPlainText().toAscii();
  QStringList dlist = defs.split(' ');

  QByteArray res = Combine().preprocess(input, dlist);

  // create a new document and fill it with data
  AbstractDocument * doc = DveDocumentFactory(this).create(root_);
  DveDocument * ddoc = static_cast<DveDocument*>(doc);

  TextEditor * ed = static_cast<TextEditor*>(ddoc->editor(ddoc->mainView()));
  ed->textDocument()->setPlainText(res);
  ed->textDocument()->setModified(true);

  ddoc->openView(ed->viewName());
  root_->activateEditor(ed);
}

void DvePlugin::onCombineTriggered()
{
  Q_ASSERT(root_->activeEditor());

  // collect all currently open LTL documents and try to save them first
  QList<AbstractEditor*> saveList;

  for(AbstractDocument* doc : root_->documents()) {
    // LTL has only one view
    if(qobject_cast<LtlDocument*>(doc))
      saveList.append(doc->editor(doc->mainView()));
  }

  if(!saveList.isEmpty() && !root_->maybeSave(saveList))
    return;

  CombineDialog dlg(root_, root_->activeEditor()->document());

  if(!dlg.exec())
    return;

  QByteArray input = qobject_cast<TextEditor*>(
    root_->activeEditor())->textDocument()->toPlainText().toAscii();
  QByteArray ltl;
  int prop;

  if(dlg.fileName().isEmpty()) {
    ltl = QString("#property ").arg(dlg.property()).toAscii();
  } else {
    QFile file(dlg.fileName());

    if(!file.open(QFile::ReadOnly | QFile::Text)) {
      QMessageBox::warning(root_, tr("DiVinE IDE"),
                           tr("Cannot open file '%1'.").arg(dlg.fileName()));
      return;
    }
    ltl = file.readAll();
    prop = dlg.propertyID();
  }

  QByteArray res = Combine().combine(input, ltl, prop);

  // create a new document and fill it with data
  AbstractDocument * doc = DveDocumentFactory(this).create(root_);
  DveDocument * ddoc = static_cast<DveDocument*>(doc);

  TextEditor * ed = static_cast<TextEditor*>(ddoc->editor(ddoc->mainView()));
  ed->textDocument()->setPlainText(res);
  ed->textDocument()->setModified(true);

  ddoc->openView(ed->viewName());
  root_->activateEditor(ed);
}

void DvePlugin::onMetricsTriggered()
{
  Meta meta;
  meta.algorithm.algorithm = divine::meta::Algorithm::Metrics;

  runAlgorithm(meta);
}

void DvePlugin::onReachabilityTriggered()
{
  Meta meta;
  meta.algorithm.algorithm = divine::meta::Algorithm::Reachability;

  runAlgorithm(meta);
}

void DvePlugin::onSearchTriggered()
{
  // TODO: implement search?
}

void DvePlugin::onVerifyTriggered()
{
  Meta meta;
  QString algorithm = Settings("ide/verification").value("algorithm", defVerificationAlgorithm).toString();
 
  if(algorithm == "OWCTY") {
    meta.algorithm.algorithm = divine::meta::Algorithm::Owcty;
  } else if(algorithm == "MAP") {
    meta.algorithm.algorithm = divine::meta::Algorithm::Map;
  } else if(algorithm == "Nested DFS") {
    meta.algorithm.algorithm = divine::meta::Algorithm::Ndfs;
  }

  runAlgorithm(meta);
}

void DvePlugin::onRunnerFinished()
{
  if(!root_->isLocked(lock_))
    return;

  // flush incomplete output
  Output::_output->progress() << std::endl;

  QProgressBar * progress = root_->findChild<QProgressBar*>("main.status.progress");
  QPushButton * button = root_->findChild<QPushButton*>("main.status.stop");

  disconnect(button, SIGNAL(clicked(bool)), runner_, SLOT(abort()));

  progress->setVisible(false);
  button->setVisible(false);

  lock_->forceRelease();

  // ask for counterexample
  if(runner_->meta().result.iniTrail != "-") {
    int res = QMessageBox::information(
      root_, tr("DiVinE IDE"), tr("A counterexample has been generated, do you wish to load it?"),
      QMessageBox::Yes | QMessageBox::Default, QMessageBox::No | QMessageBox::Escape);

    if(res != QMessageBox::Yes)
      return;

    if(!root_->openDocument(runner_->meta().input.model.c_str()))
      return;

    // prepare the sequence
    QVector<int> cev;
    cev += extract(runner_->meta().result.iniTrail.c_str());
    cev += extract(runner_->meta().result.cycleTrail.c_str());

    root_->simulation()->start();
    root_->simulation()->autoRun(cev);
  }
}

void DvePlugin::runAlgorithm(const Meta & meta)
{
  // just in case
  if(root_->isLocked())
    return;

  AbstractEditor * editor = root_->activeEditor();
  Q_ASSERT(editor);

  if(!root_->maybeSave(editor))
    return;

  lock_->lock(editor->document());
  Q_ASSERT(qobject_cast<DveDocument*>(editor->document()));

  Meta mt = meta;
  mt.input.model = editor->document()->path().toStdString();
  mt.execution.threads = Settings("ide/verification").value("threads", defVerificationThreads).toInt();
  mt.output.wantCe = true;
  mt.output.statistics = false;

  // the fancy bars
  QProgressBar * progress = root_->findChild<QProgressBar*>("main.status.progress");
  QPushButton * button = root_->findChild<QPushButton*>("main.status.stop");

  Q_ASSERT(progress && button);

  progress->setMinimum(0);
  progress->setMaximum(0);
  progress->setValue(0);
  progress->setVisible(true);
// TODO: enable when early termination is implemented
//   button->setVisible(true);

  runner_->init(mt);
  connect(runner_, SIGNAL(finished()), SLOT(onRunnerFinished()));
  connect(button, SIGNAL(clicked(bool)), runner_, SLOT(abort()));
  runner_->start();
}

}
}

Q_EXPORT_PLUGIN2(dvePlugin, divine::gui::DvePlugin)
