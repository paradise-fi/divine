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

#include <QtPlugin>
#include <QAction>
#include <QMenu>
#include <QToolBar>
#include <QProcess>
#include <QUrl>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>

#include "dve_divine2_plugin.h"
#include "dve_simulator.h"
#include "mainform.h"
#include "editor.h"
#include "prefs_divine.h"
#include "settings.h"
#include "combine_dlg.h"

Simulator * DveSimulatorFactory::load(const QString & fileName, MainForm * root)
{
  DveSimulator * simulator = new DveSimulator(root);

  if (!simulator)
    return NULL;
  
  // try to open the file
  if (!fileName.isEmpty()) 
    simulator->loadFile(fileName);

  return simulator;
}

void DvePlugin::install(MainForm * root)
{
  root_ = root;
  
  // document loaders
  SimulatorFactory * factory = new DveSimulatorFactory(root);
  root->registerSimulator("dve", factory);
  
  combineAct_ = new QAction(tr("C&ombine..."), this);
  combineAct_->setObjectName("combineAct");
  combineAct_->setStatusTip(tr("Combines current model with LTL formula"));
  connect(combineAct_, SIGNAL(triggered()), SLOT(combine()));
  
  reachabilityAct_ = new QAction(tr("&Reachability"), this);
  reachabilityAct_->setObjectName("reachabilityAct");
  reachabilityAct_->setStatusTip(tr("Perform reachability analysis"));
  connect(reachabilityAct_, SIGNAL(triggered()), SLOT(reachability()));
    
  metricsAct_ = new QAction(tr("&Metrics"), this);
  metricsAct_->setObjectName("metricsAct");
  metricsAct_->setStatusTip(tr("Analyze state space metrics"));
  connect(metricsAct_, SIGNAL(triggered()), SLOT(metrics()));
  
  verifyAct_ = new QAction(tr("&Verify LTL"), this);
  verifyAct_->setObjectName("verifyAct");
  verifyAct_->setStatusTip(tr("Verify a model with LTL formula"));
  connect(verifyAct_, SIGNAL(triggered()), SLOT(verify()));
  
  abortAct_ = new QAction(tr("&Abort"), this);
  abortAct_->setObjectName("abortAct");
  abortAct_->setStatusTip(tr("Abort computation"));
  abortAct_->setEnabled(false);
  connect(abortAct_, SIGNAL(triggered()), SLOT(abort()));
  
  QMenu * algorithmMenu = new QMenu(tr("&Algorithm"));
  algorithmMenu->setObjectName("AlgorithmMenu");
  algorithmMenu->setStatusTip(tr("Select default verification algorithm"));
  
  algorithmGroup_ = new QActionGroup(algorithmMenu);
  
  QAction * action = algorithmGroup_->addAction(tr("&OWCTY"));
  action->setStatusTip(tr("One-way-catch-them-young algorithm"));
  action->setData("owcty");
  action->setCheckable(true);
  
  action = algorithmGroup_->addAction(tr("Nested &DFS"));
  action->setStatusTip(tr("Nested DFS algorithm"));
  action->setData("nested-dfs");
  action->setCheckable(true);
  
  action = algorithmGroup_->addAction(tr("&MAP"));
  action->setStatusTip(tr("Maximum accepting predecessor algorithm"));
  action->setData("map");
  action->setCheckable(true);
  
  algorithmMenu->addActions(algorithmGroup_->actions());
  
  sSettings().beginGroup("DiVinE");
  int alg = qBound(0, sSettings().value("algorithm", defDivAlgorithm).toInt(), 2);
  algorithmGroup_->actions().at(alg)->setChecked(true);
  sSettings().endGroup();
  
  connect(algorithmMenu, SIGNAL(triggered(QAction*)), SLOT(onAlgorithmTriggered(QAction*)));
  
  QMenu * menu = root->findChild<QMenu*>("toolsMenu");
  QAction * sep = root->findChild<QAction*>("toolsSeparator");
  Q_ASSERT(menu);
  Q_ASSERT(sep);
  
  menu->insertAction(sep, combineAct_);
  menu->insertSeparator(sep);
  menu->insertAction(sep, reachabilityAct_);
  menu->insertAction(sep, metricsAct_);
  menu->insertAction(sep, verifyAct_);
  menu->insertSeparator(sep);
  menu->insertAction(sep, abortAct_);
  menu->insertSeparator(sep);
  menu->insertMenu(sep, algorithmMenu);
  
  sep->setVisible(true);

  menu = root->findChild<QMenu*>("toolbarsMenu");
  Q_ASSERT(menu);

  QToolBar * toolbar = root->addToolBar(tr("Verification"));
  toolbar->setObjectName("divine2ToolBar");
  toolbar->addAction(reachabilityAct_);
  toolbar->addAction(verifyAct_);
  toolbar->addSeparator();
  toolbar->addAction(abortAct_);

  action = toolbar->toggleViewAction();
  action->setText(tr("&Verification"));
  action->setStatusTip(tr("Toggles the verification toolbar"));
  menu->addAction(action);
  
  PreferencesPage * page = new DivinePreferences();
  root->registerPreferences(QObject::tr("Tools"), QObject::tr("DiVinE"), page);
  
  connect(this, SIGNAL(message(QString)), root, SIGNAL(message(QString)));
  connect(root, SIGNAL(editorChanged(SourceEditor*)), SLOT(onEditorChanged(SourceEditor*)));
  
  onEditorChanged(root->activeEditor());
}

void DvePlugin::prepareDivine(QStringList & args)
{
  QSettings & s = sSettings();
  
  s.beginGroup("DiVinE");
  
  args.append(QString("--workers=%1").arg(s.value("threads", defDivThreads).toInt()));
  args.append(QString("--max-memory=%1").arg(s.value("memory", defDivMemory).toInt()));
  
  if(s.value("nopool", defDivNoPool).toBool())
    args.append("--disable-pool");

  args.append("--report");
  
  s.endGroup();
}

void DvePlugin::runDivine(const QString & algorithm)
{
  Q_ASSERT(root_->activeEditor());
  
  if(!root_->maybeSave(root_->activeEditor()))
    return;
  
  // never saved
  QUrl url(root_->activeEditor()->document()->metaInformation(QTextDocument::DocumentUrl));
  if(url.path().isEmpty())
    return;

  QStringList args;
  QString program;
  
  prepareDivine(args);
  
  args.append(algorithm);
  args.append(url.path());
  
  sSettings().beginGroup("DiVinE");
  program = sSettings().value("path", defDivPath).toString();
  sSettings().endGroup();
 
  divineProcess_ = new QProcess(this);
  divineFile_ = url.path();
  
  QByteArray array(root_->activeEditor()->toPlainText().toAscii());
  divineCRC_ = qChecksum(array.data(), array.size());
  
  connect(divineProcess_, SIGNAL(error(QProcess::ProcessError)), SLOT(onRunnerError(QProcess::ProcessError)));
  connect(divineProcess_, SIGNAL(finished(int,QProcess::ExitStatus)), SLOT(onRunnerFinished(int,QProcess::ExitStatus))); 
  
  emit message(tr("=== Running divine - %1 algorithm ===").arg(algorithm));
  
  divineProcess_->start(program, args, QIODevice::ReadOnly);
  abortAct_->setEnabled(true);
  
  onEditorChanged(root_->activeEditor());
}

void DvePlugin::combine(void)
{
  Q_ASSERT(root_->activeEditor());
  
  QList<SourceEditor*> elist;
  elist.append(root_->activeEditor());

  for(int i = 0; i < root_->editorCount(); ++i) {
    SourceEditor * editor = root_->editor(i);
    QUrl url = editor->document()->metaInformation(QTextDocument::DocumentUrl);
    if(url.scheme() == "ltl")
      elist.append(editor);
  }
  
  // try to save current LTL files
  if(!root_->maybeSave(elist))
    return;
  
  // never saved
  QUrl url(root_->activeEditor()->document()->metaInformation(QTextDocument::DocumentUrl));
  Q_ASSERT(!url.path().isEmpty());
  
  CombineDialog dlg(root_, url.path());
  
  if(!dlg.exec())
    return;

  QStringList args;
  QString program;
  
  sSettings().beginGroup("DiVinE");
  program = sSettings().value("path", defDivPath).toString();
  sSettings().endGroup();
  
  args.append("combine");
  
  args.append("-o");
  
  if(!dlg.file().isEmpty()) {
    args.append("-p");
    args.append(QString::number(dlg.property() + 1));
    args.append("-f");
    args.append(dlg.file());
  }
  args.append(url.path());
  args.append(dlg.definitions());
  
  emit message(tr("=== Running divine combine ==="));
  
  QProcess process;
  process.start(program, args, QIODevice::ReadOnly);
  process.waitForStarted();
  
  if(process.state() == QProcess::NotRunning && process.error() == QProcess::FailedToStart) {
    emit message(tr("Error invoking divine (%1)").arg(program));
    return;
  }
  
  process.waitForFinished();
  
  QByteArray errors = process.readAllStandardError();
  QByteArray output = process.readAllStandardOutput();
  
  if(!errors.isEmpty())
    emit message(errors);
  
  if(process.exitCode() == 0)
    root_->newFile("dve", output);
}

void DvePlugin::reachability(void)
{
  runDivine("reachability");
}

void DvePlugin::metrics(void)
{
  runDivine("metrics");
}

void DvePlugin::verify(void)
{
  QAction * action = algorithmGroup_->checkedAction();
  Q_ASSERT(action);
  
  runDivine(action->data().toString());
}

void DvePlugin::abort(void)
{
  if(divineProcess_) {
    divineProcess_->terminate();
    
    divineProcess_->deleteLater();
    divineProcess_ = NULL;
  }
}

void DvePlugin::onRunnerError(QProcess::ProcessError error)
{
  sSettings().beginGroup("DiVinE");
  QString program = sSettings().value("path", defDivPath).toString();
  sSettings().endGroup();
  
  QString reason;
  switch (error) {
    case QProcess::FailedToStart:
      reason = tr("Process failed to start");
      break;
    case QProcess::Crashed:
      reason = tr("Process crashed");
      break;
    default:
      reason = tr("Unknown");
      break;
  }
  
  emit message(tr("Error running divine (%1)\nReason: %2").arg(program, reason));
  
  abortAct_->setEnabled(false);
  
  if(divineProcess_) {
    divineProcess_->deleteLater();
    divineProcess_ = NULL;
  }
  
  // update menu options
  onEditorChanged(root_->activeEditor());
}

void DvePlugin::onRunnerFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
  // error occured and process has already been deleted
  if(!divineProcess_)
    return;
  
  abortAct_->setEnabled(false);
  
  // retrieve output from divine process
  QByteArray report = divineProcess_->readAllStandardOutput();
  QByteArray log = divineProcess_->readAllStandardError();
  
  divineProcess_->deleteLater();
  divineProcess_ = NULL;
  
  // update menu options
  onEditorChanged(root_->activeEditor());
  
  sSettings().beginGroup("DiVinE");
  bool printReport = sSettings().value("report", defDivReport).toBool();
  sSettings().endGroup();
  
  if(printReport && !report.isEmpty())
    emit message(report);
  
  if(!log.isEmpty())
    emit message(log);
  
  // extract trail from report
  QRegExp trailRe1("^Trail-Init: (.*)$");
  QRegExp trailRe2("^Trail-Cycle: (.*)$");
  
  QStringList lines = QString(report).split('\n');
  QList<int> trail;
  
  if(lines.indexOf(trailRe1) != -1) {
    QString line = trailRe1.capturedTexts().at(1);
    if(line.simplified() != "-") {
      QStringList list = line.simplified().split(',');
      foreach(QString str, list) {
        trail.append(str.toInt());
      }
    }
  }
  
  if(lines.indexOf(trailRe2) != -1) {
    QString line = trailRe2.capturedTexts().at(1);
    if(line.simplified() != "-") {
      QStringList list = line.simplified().split(',');
      foreach(QString str, list) {
        trail.append(str.toInt());
      }
    }
  }
  
  if(trail.isEmpty())
    return;
  
  // check if the file has changed since the start of the process
  SourceEditor * editor = root_->editor(divineFile_);
  bool changed = false;
  
  QByteArray array;
  
  if(editor) {
    array = editor->toPlainText().toAscii();
    
    changed = editor->document()->isModified() ||
      qChecksum(array.data(), array.size()) != divineCRC_;
  } else {
    QFile file(divineFile_);
    file.open(QFile::ReadOnly | QFile::Text);

    QTextStream in(&file);
    array = in.readAll().toAscii();
    
    changed = divineCRC_ != qChecksum(array.data(), array.size());
    
  }
    
  int res;
  
  if(changed) {
    res = QMessageBox::warning(
      root_, tr("DiVinE IDE"), tr("A counterexample has been generated. However "
        "the source file has been changed since the start of the algorithm. Do do you wish to load it?"),        
      QMessageBox::Yes | QMessageBox::Default, QMessageBox::No | QMessageBox::Escape);
  } else {
    res = QMessageBox::information(
      root_, tr("DiVinE IDE"), tr("A counterexample has been generated, do you wish to load it?"),        
      QMessageBox::Yes | QMessageBox::Default, QMessageBox::No | QMessageBox::Escape);
  }
  
  if(res == QMessageBox::Yes && root_->maybeStop()) {
    root_->openFile(divineFile_);
    
    if(root_->activeEditor() != root_->editor(divineFile_))
      return;

    root_->startSimulation();
    
    foreach(int idx, trail) {
      if(idx <= 0 || idx > root_->simulator()->simulator()->transitionCount()) {
        QMessageBox::warning(root_, tr("DiVinE IDE"), tr("Invalid trail!"));
        root_->simulator()->stop();
        return;
      }
      root_->simulator()->step(idx - 1);
    }
  }
}

void DvePlugin::onEditorChanged(SourceEditor * editor)
{
  QUrl url;
  
  if(editor)
    url = editor->document()->metaInformation(QTextDocument::DocumentUrl);
  
  bool isDve = url.scheme() == "dve";
  bool isMDve = url.scheme() == "mdve";

  combineAct_->setEnabled(isDve || isMDve);
  
  // only one parallel process is allowed
  reachabilityAct_->setEnabled(isDve && !divineProcess_);
  metricsAct_->setEnabled(isDve && !divineProcess_);
  verifyAct_->setEnabled(isDve && !divineProcess_);
}

void DvePlugin::onAlgorithmTriggered(QAction * action)
{
  sSettings().beginGroup("DiVinE");
  
  int index = algorithmGroup_->actions().indexOf(action);
  sSettings().setValue("algorithm", index);
  
  sSettings().endGroup();
}

Q_EXPORT_PLUGIN2(dve_legacy, DvePlugin)
