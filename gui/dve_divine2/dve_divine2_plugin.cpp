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

Simulator * DveSimulatorLoader::load(const QString & fileName, MainForm * root)
{
  DveSimulator * simulator = new DveSimulator(root);

  if (!simulator)
    return NULL;

  connect(simulator, SIGNAL(message(QString)), root, SIGNAL(message(QString)));
  
  // neither new file, nor successfully loaded
  if (!fileName.isEmpty() && !simulator->openFile(fileName)) {
    delete simulator;
    return NULL;
  }

  return simulator;
}

void DvePlugin::install(MainForm * root)
{
  root_ = root;
  
  // document loaders
  SimulatorLoader * loader = new DveSimulatorLoader(root);
  root->registerSimulator("dve", loader);

  syntaxAct_ = new QAction(tr("Check &Syntax"), this);
  syntaxAct_->setObjectName("syntaxAct");
  syntaxAct_->setStatusTip(tr("Check model syntax"));
  connect(syntaxAct_, SIGNAL(triggered()), SLOT(checkSyntax()));
  
  algorithmMenu_ = new QMenu(tr("&Algorithm"));
  QAction * action = algorithmMenu_->addAction(tr("&Reachability"));
  action->setStatusTip(tr("Perform reachability analysis"));
  action->setData("reachability");
  
  action = algorithmMenu_->addAction(tr("Me&trics"));
  action->setStatusTip(tr("Analyze state space metrics"));
  action->setData("metrics");
  
  action = algorithmMenu_->addAction(tr("&OWCTY"));
  action->setStatusTip(tr("Verify model using the One-way-catch-them-young algorithm"));
  action->setData("owcty");
  
  action = algorithmMenu_->addAction(tr("Nested &DFS"));
  action->setStatusTip(tr("Verify model using the nested DFS algorithm"));
  action->setData("nested-dfs");
  
  action = algorithmMenu_->addAction(tr("&MAP"));
  action->setStatusTip(tr("Verify model using the Maximum accepting predecessor algorithm"));
  action->setData("map");
  
  connect(algorithmMenu_, SIGNAL(triggered(QAction*)), SLOT(runAlgorithm(QAction*)));

  combineAct_ = new QAction(tr("&Combine..."), this);
  combineAct_->setObjectName("combineAct");
  combineAct_->setStatusTip(tr("Combines current model with LTL formula"));
  connect(combineAct_, SIGNAL(triggered()), SLOT(combine()));
  
  QMenu * menu = root->findChild<QMenu*>("toolsMenu");
  QAction * sep = root->findChild<QAction*>("toolsSeparator");
  Q_ASSERT(menu);
  
  menu->insertAction(sep, syntaxAct_);
  menu->insertAction(sep, combineAct_);
  menu->insertMenu(sep, algorithmMenu_);
  menu->setEnabled(true);
  
  PreferencesPage * page = new DivinePreferences();
  root->registerPreferences(QObject::tr("Tools"), QObject::tr("DiVinE Tool"), page);
  
  connect(this, SIGNAL(message(QString)), root, SIGNAL(message(QString)));
  connect(root, SIGNAL(editorChanged(SourceEditor*)), SLOT(onEditorChanged(SourceEditor*)));
}

void DvePlugin::prepareDivine(QStringList & args)
{
  QSettings & s = sSettings();
  
  s.beginGroup("DiVinE Tool");
  
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
  
  sSettings().beginGroup("DiVinE Tool");
  program = sSettings().value("path", defDivPath).toString();
  sSettings().endGroup();
 
  divineProcess_ = new QProcess(this);
  divineFile_ = url.path();
  
  QByteArray array(root_->activeEditor()->toPlainText().toAscii());
  divineCRC_ = qChecksum(array.data(), array.size());
  
  connect(divineProcess_, SIGNAL(error(QProcess::ProcessError)), SLOT(onRunnerError(QProcess::ProcessError)));
  connect(divineProcess_, SIGNAL(finished(int,QProcess::ExitStatus)), SLOT(onRunnerFinished(int,QProcess::ExitStatus)));
  
  emit message(tr("Running %1 algorithm...").arg(algorithm));
  
  divineProcess_->start(program, args, QIODevice::ReadOnly);
}

void DvePlugin::checkSyntax(void)
{
  Q_ASSERT(root_->activeEditor());
  
  if(!root_->maybeSave(root_->activeEditor()))
    return;
  
  // never saved
  QUrl url(root_->activeEditor()->document()->metaInformation(QTextDocument::DocumentUrl));
  if(url.path().isEmpty())
    return;
  
  DveSimulator::checkSyntax(url.path());
}

void DvePlugin::combine(void)
{
  Q_ASSERT(root_->activeEditor());
  
  if(!root_->maybeSaveAll())
    return;
  
  // never saved
  QUrl url(root_->activeEditor()->document()->metaInformation(QTextDocument::DocumentUrl));
  if(url.path().isEmpty())
    return;
  
  CombineDialog dlg(root_);
  
  if(!dlg.exec())
    return;

  QStringList args;
  QString program;
  
  sSettings().beginGroup("DiVinE Tool");
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
  
  if(process.exitCode() == 0) {
    root_->newFile("dve");
    root_->activeEditor()->setPlainText(output);
    root_->activeEditor()->document()->setModified(true);
  }
}

void DvePlugin::runAlgorithm(QAction * action)
{
  Q_ASSERT(action);
  
  QString algorithm = action->data().toString();
  runDivine(algorithm);
}

void DvePlugin::onRunnerError(QProcess::ProcessError error)
{
  if(error == QProcess::FailedToStart) {
    sSettings().beginGroup("DiVinE Tool");
    QString program = sSettings().value("path", defDivPath).toString();
    sSettings().endGroup();
    
    emit message(tr("Error invoking divine (%1)").arg(program));
  }

  qDebug("process error: %d", error);
}

void DvePlugin::onRunnerFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
  // retrieve output from divine process
  QByteArray report = divineProcess_->readAllStandardOutput();
  QByteArray log = divineProcess_->readAllStandardError();
  
  delete divineProcess_;
  divineProcess_ = NULL;
  
  // update menu options
  onEditorChanged(root_->activeEditor());
  
  sSettings().beginGroup("DiVinE Tool");
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
  QUrl url(editor->document()->metaInformation(QTextDocument::DocumentUrl));
  
  bool isDve = url.scheme() == "dve";
  bool isMDve = url.scheme() == "mdve";

  syntaxAct_->setEnabled(isDve);
  combineAct_->setEnabled(isDve || isMDve);
  
  // only one parallel process is allowed
  algorithmMenu_->setEnabled(isDve && !divineProcess_);
}

Q_EXPORT_PLUGIN2(dve_legacy, DvePlugin)
