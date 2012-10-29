/***************************************************************************
 *   Copyright (C) 2012 by Martin Moracek                                  *
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

#include <QtXml>

#include <QAction>
#include <QMenu>
#include <QToolBar>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QProgressBar>
#include <QPushButton>

#include "abstractEditor.h"
#include "abstractDocument.h"
#include "abstractToolLock.h"
#include "plugins.h"
#include "pluginManager.h"
#include "mainForm.h"
#include "settings.h"
#include "layoutManager.h"
#include "simulationProxy.h"
#include "simulationTools.h"
#include "abstractSimulator.h"
#include "signalLock.h"

namespace divine {
namespace gui {

namespace {
  void normalizeIndex(int & index, int current)
  {
    if(index < 0)
      index = current;
  }
}

SimulationProxy::SimulationProxy(MainForm * parent)
  : QObject(parent), root_(parent), simulator_(NULL), lock_(NULL),
  loader_(NULL), cycle_(-1, -1), index_(-1), readOnly_(false)
{
  lock_ = new SimulationLock(root_, this);
  loader_ = new SimulationLoader(this);

  connect(loader_, SIGNAL(step(int)), SLOT(autoStep(int)), Qt::BlockingQueuedConnection);
  connect(loader_, SIGNAL(finished()), SLOT(autoFinished()), Qt::BlockingQueuedConnection);

  createActions();
  createMenus();
  createToolbars();

  // make sure we are updating actions as the editors are switched around
  connect(root_, SIGNAL(editorDeactivated()), SLOT(updateActions()));
  connect(root_, SIGNAL(editorActivated(AbstractEditor*)), SLOT(updateActions()));
  connect(root_, SIGNAL(lockChanged()), SLOT(updateActions()));

  updateActions();
}

SimulationProxy::~SimulationProxy()
{
  stop();
}

bool SimulationProxy::isRunning() const
{
  return simulator_ != NULL;
}

int SimulationProxy::currentIndex() const
{
  return index_;
}

int SimulationProxy::stateCount() const
{
  return isRunning() ? simulator_->stateCount() : 0;
}

bool SimulationProxy::isAccepting(int state)
{
  normalizeIndex(state, index_);
  return isValidIndex(state) && simulator_->isAccepting(state);
}

bool SimulationProxy::isValid(int state)
{
  normalizeIndex(state, index_);
  return isValidIndex(state) && simulator_->isValid(state);
}

int SimulationProxy::transitionCount(int state)
{
  normalizeIndex(state, index_);
  return isValidIndex(state) ? simulator_->transitionCount(state) : 0;
}

const Transition * SimulationProxy::transition(int index, int state)
{
  normalizeIndex(state, index_);
  return isValidIndex(state) && index >= 0 && index < transitionCount(state) ?
    simulator_->transition(index, state) : NULL;
}

QList<const Communication*> SimulationProxy::communication(int index, int state)
{
  normalizeIndex(state, index_);
  return isValidIndex(state) && index >= 0 && index < transitionCount(state) ?
    simulator_->communication(index, state) : QList<const Communication*>();
}

QList<const Transition*> SimulationProxy::transitions(int state)
{
  normalizeIndex(state, index_);
  return isValidIndex(state) ? simulator_->transitions(state) : QList<const Transition*>();
}

int SimulationProxy::usedTransition(int state)
{
  normalizeIndex(state, index_);
  return isValidIndex(state) ? simulator_->usedTransition(state) : -1;
}

const SymbolType * SimulationProxy::typeInfo(const QString & id) const
{
  return isRunning() ? simulator_->typeInfo(id) : NULL;
}

const Symbol * SimulationProxy::symbol(const QString & id, int state)
{
  normalizeIndex(state, index_);
  return isValidIndex(state) ? simulator_->symbol(id, state) : NULL;
}

QList<QString> SimulationProxy::topLevelSymbols(int state)
{
  normalizeIndex(state, index_);
  return isValidIndex(state) ? simulator_->topLevelSymbols(state) : QList<QString>();
}

QVariant SimulationProxy::symbolValue(const QString & id, int state)
{
  normalizeIndex(state, index_);
  return isValidIndex(state) ? simulator_->symbolValue(id, state) : QVariant();
}

bool SimulationProxy::setSymbolValue
(const QString & id, const QVariant & value, int state)
{
  if(isReadOnly())
    return false;

  normalizeIndex(state, index_);
  if(!isValidIndex(state))
    return false;

  if(state < 0)
    state = simulator_->stateCount() - 1;

  if(simulator_->setSymbolValue(id, value, state)) {
    // invalidate accepting cycle
    cycle_ = qMakePair(-1, -1);

    // move to valid region
    if(index_ > state)
      setCurrentIndex(state);

    // announce first, then remove
    int count = simulator_->stateCount();
    if(state + 1 <= count - 1)
      emit stateRemoved(state + 1, count - 1);

    simulator_->trimStack(state);

    cycle_ = simulator_->findAcceptingCycle();
    emit acceptingCycleChanged(cycle_);
    return true;
  }
  return false;
}

void SimulationProxy::autoRun(const QVector<int> & steps, int delay)
{
  // disable user control
  readOnly_ = true;

  // the fancy bars
  QProgressBar * progress = root_->findChild<QProgressBar*>("main.status.progress");
  QPushButton * button = root_->findChild<QPushButton*>("main.status.stop");

  Q_ASSERT(progress && button);

  progress->setMinimum(0);
  progress->setMaximum(steps.count());
  progress->setValue(0);
  progress->setVisible(true);
  button->setVisible(true);

  loader_->init(steps, delay);
  connect(loader_, SIGNAL(progress(int)), progress, SLOT(setValue(int)), Qt::BlockingQueuedConnection);
  connect(button, SIGNAL(clicked(bool)), loader_, SLOT(abort()), Qt::BlockingQueuedConnection);
  loader_->start();
}

void SimulationProxy::start()
{
  Q_ASSERT(!isRunning());
  Q_ASSERT(!isReadOnly());

  if(root_->isLocked())
    return;

  AbstractEditor * editor = root_->activeEditor();
  Q_ASSERT(editor);

  // try to save all editors belonging to the current document
  QStringList views = editor->document()->views();
  QList<AbstractEditor*> editors;

  foreach(QString view, views) {
    editors.append(editor->document()->editor(view));
  }

  if(!root_->maybeSave(editors))
    return;

  lock_->lock(editor->document());

  // create a new simulator instance
  const AbstractSimulatorFactory * fab = root_->plugins()->findSimulator(
    QFileInfo(editor->document()->path()).suffix());

  Q_ASSERT(fab);

  simulator_ = fab->create(root_);

  if(!simulator_->loadDocument(editor->document())) {
    delete simulator_;
    simulator_ = NULL;

    lock_->forceRelease();
    return;
  }

  // load settings
  Settings s("ide/simulator");

  if (s.value("random", defSimulatorRandom).toBool()) {
    qsrand(s.value("seed", defSimulatorSeed).toUInt());
  } else {
    qsrand(QTime::currentTime().msec());
  }

  connect(simulator_, SIGNAL(stateChanged(int)), SIGNAL(stateChanged(int)));
  connect(simulator_, SIGNAL(stateChanged(int)), SLOT(updateActions()));

  // emitting reset
  SignalLock lock(simulator_);

  root_->layouts()->switchLayout("debug");
  simulator_->start();

  index_ = 0;
  cycle_ = simulator_->findAcceptingCycle();

  emit started();
  emit reset();

  updateActions();
}

void SimulationProxy::stop()
{
  if(isReadOnly() || !isRunning())
    return;

  cycle_ = qMakePair(-1, -1);
  emit acceptingCycleChanged(cycle_);
  emit stateRemoved(0, simulator_->stateCount() - 1);

  simulator_->stop();
  delete simulator_;
  simulator_ = NULL;

  emit stopped();

  root_->layouts()->switchLayout("edit");
  lock_->forceRelease();

  index_ = -1;

  updateActions();
}

void SimulationProxy::restart()
{
  if(isReadOnly() || !isRunning())
    return;

  setCurrentIndex(0);
  cycle_ = qMakePair(-1, -1);

  int count = simulator_->stateCount();

  simulator_->trimStack(0);

  SignalLock lock(simulator_);
  simulator_->stop();
  simulator_->start();

  cycle_ = simulator_->findAcceptingCycle();

  emit reset();

  updateActions();
}

void SimulationProxy::setCurrentIndex(int index)
{
  if(isReadOnly() || !isValidIndex(index))
    return;

  if(index_ == index)
    return;

  index_ = index;
  emit currentIndexChanged(index_);

  updateActions();
}

void SimulationProxy::next()
{
  setCurrentIndex(index_ + 1);
}

void SimulationProxy::previous()
{
  setCurrentIndex(index_ - 1);
}

void SimulationProxy::step(int id)
{
  if(isReadOnly() || !isRunning())
    return;

  if(!(id >= 0 && id < simulator_->transitionCount(index_)))
    return;

  // check if we are trying transition used before
  if(id == simulator_->usedTransition(index_)) {
    next();
    return;
  }

  // NOTE: do we want this feature here? if we continue after the cycle and
  // deviate from it, it will still remain on the stack. with loop-jump,
  // we go back into the cycle and any deviation invokes trimming

  // NOTE: for consistency's sake, we might also detect all cycles and jump
  // whenever we loop (probably overkill)

  // check if we have accepting cycle and are looping
//   if(cycle_ != qMakePair(-1, -1) && cycle_.second == index_ &&
//      usedTransition(cycle_.first) == id) {
//     setCurrentIndex(cycle_.first + 1);
//     return;
//   }

  // if not trim the stack
  int last = simulator_->stateCount() - 1;
  simulator_->trimStack(index_);

  if(index_ + 1 <= last)
    emit stateRemoved(index_ + 1, last);

  simulator_->step(id);

  last = simulator_->stateCount() - 1;
  emit stateAdded(last, last);
  setCurrentIndex(last);

  cycle_ = simulator_->findAcceptingCycle();
  emit acceptingCycleChanged(cycle_);
}

void SimulationProxy::random()
{
  if(isReadOnly() || !isRunning())
    return;

  int pick = qrand() % simulator_->transitionCount(index_);

  step(pick);
}

void SimulationProxy::updateActions()
{
  AbstractEditor * editor = root_->activeEditor();

  if(isReadOnly() || root_->isLocked() || !editor) {
    startAct_->setEnabled(false);
  } else {
    bool canSimulate = root_->plugins()->findSimulator(QFileInfo(
      editor->document()->path()).suffix()) != NULL;

    startAct_->setEnabled(canSimulate);
  }

  stopAct_->setEnabled(isRunning() && !isReadOnly());
  restartAct_->setEnabled(isRunning() && !isReadOnly());
  nextAct_->setEnabled(isRunning() && !isReadOnly() && index_ < simulator_->stateCount() - 1);
  prevAct_->setEnabled(isRunning() && !isReadOnly() && index_ > 0);
  randomAct_->setEnabled(isRunning() && !isReadOnly() && transitionCount() > 0);
  randomRunAct_->setEnabled(isRunning() && !isReadOnly() && transitionCount() > 0);

  importAct_->setEnabled(!isReadOnly());
  exportAct_->setEnabled(isRunning() && !isReadOnly());
}

void SimulationProxy::randomRun()
{
  // must be running and NOT doing anything else
  if(!isRunning() || isReadOnly())
    return;

  Settings s("ide/simulator");
  int steps = s.value("steps", defSimulatorSteps).toInt();
  int delay = s.value("delay", defSimulatorDelay).toInt();

  bool ok;
  steps = QInputDialog::getInteger(root_, tr("Random run"),
    tr("Select number of steps to run:"), steps, 1, 800, 1, &ok);

  if (!ok)
    return;

  // trim the stack
  int last = simulator_->stateCount() - 1;
  simulator_->trimStack(index_);

  if(index_ + 1 <= last)
    emit stateRemoved(index_ + 1, last);

  autoRun(QVector<int>(steps, -1), delay);
}

void SimulationProxy::importRun()
{
  if(!root_->maybeStopAll())
    return;

  QString fileName = QFileDialog::getOpenFileName(root_, tr("Import run"),
    "", tr("Stack trace") + "(*.xml) (*.xml)");

  if(fileName.isEmpty())
    return;

  QFile file(fileName);
  if(!file.open(QFile::ReadOnly | QFile::Text)) {
    QMessageBox::warning(root_, tr("DiVinE IDE"),
                         tr("Cannot open file '%1'.").arg(fileName));
    return;
  }

  QDomDocument doc;
  doc.setContent(file.readAll());

  file.close();

  // get document
  QDomElement root = doc.documentElement();
  QString docName = root.firstChildElement("document").text();

  // we can either load the original document, or have some document already
  // open. in that case we check if filenames match and prompt the user only
  // in case they seem to be totally different (ei. not just different path)
  if(!QFileInfo(docName).exists()) {
    if(!root_->activeEditor()) {
      QMessageBox::warning(root_, tr("DiVinE IDE"),
                           tr("Unable to locate the reference document '%1'. "
                              "You have to open it manually and try again.").arg(docName));
      return;
    }

    if(QFileInfo(docName).baseName() != QFileInfo(root_->activeEditor()->document()->path()).baseName()) {
      if(QMessageBox::warning(root_, tr("DiVinE IDE"),
                              tr("The current document has different name than the reference document. "
                                 "Simulation might yield unexpected results. Proceed?"),
                              QMessageBox::Yes | QMessageBox::Default,
                              QMessageBox::No | QMessageBox::Escape) != QMessageBox::Yes) {
        return;
      }
    }
  } else {
    if(!root_->openDocument(docName))
      return;
  }

  QVector<int> steps;

  QDomElement el = root.firstChildElement("trace").firstChildElement("step");
  while(!el.isNull()) {
    int trans = el.text().toInt();

    if(trans < 0) {
      QMessageBox::warning(root_, tr("DiVinE IDE"),
                           tr("Invalid transition in imported file."));
      return;
    }
    steps.append(trans);
    el = el.nextSiblingElement("step");
  }

  // TODO: xml validation
  start();
  autoRun(steps);
}

void SimulationProxy::exportRun()
{
  if(!isRunning())
    return;

  QString fileName = QFileDialog::getSaveFileName(root_, tr("Export run"),
    "", tr("Stack trace") + "(*.xml) (*.xml)");

  if(fileName.isEmpty())
    return;

  if(QFileInfo(fileName).suffix() != "xml")
    fileName.append(".xml");

  QDomDocument doc;
  QDomElement root, group, el;

  // document root
  root = doc.createElement("simulation");
  root.setAttribute("version", "1.0");
  doc.appendChild(root);

  // document path
  el = doc.createElement("document");
  el.appendChild(doc.createTextNode(lock_->document()->path()));
  root.appendChild(el);

  // creator information
  group = doc.createElement("meta");
  el = doc.createElement("generator");
  el.appendChild(doc.createTextNode("Interactive debugger"));
  group.appendChild(el);
  root.appendChild(group);

  // trace
  group = doc.createElement("trace");

  for(int i = 0; i < simulator_->stateCount() - 1; ++i) {
    el = doc.createElement("step");
    el.appendChild(doc.createTextNode(QString::number(simulator_->usedTransition(i))));
    group.appendChild(el);
  }
  root.appendChild(group);

  QFile file(fileName);
  if(file.open(QIODevice::WriteOnly)) {
    file.write("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
    file.write(doc.toByteArray());
    file.close();
  }
}

void SimulationProxy::autoStep(int id)
{
  Q_ASSERT(isRunning());

  int transitions = simulator_->transitionCount(simulator_->stateCount() - 1);

  // deadlock
  if(transitions == 0) {
    QMessageBox::warning(root_, tr("DiVinE IDE"), tr("Reached a deadlock."));
    loader_->abort();
    return;
  }

  // get a random number
  if(id == -1) {
    id = qrand() % simulator_->transitionCount(simulator_->stateCount() - 1);
  }

  if(id < 0 || id >= transitions) {
    QMessageBox::warning(root_, tr("DiVinE IDE"), tr("Invalid transition."));
    loader_->abort();
    return;
  }
  simulator_->step(id);
}

void SimulationProxy::autoFinished()
{
  // read only must be true for auto modes
  if(!isRunning() || !isReadOnly())
    return;

  QProgressBar * progress = root_->findChild<QProgressBar*>("main.status.progress");
  QPushButton * button = root_->findChild<QPushButton*>("main.status.stop");

  disconnect(loader_, SIGNAL(progress(int)), progress, SLOT(setValue(int)));
  disconnect(button, SIGNAL(clicked(bool)), loader_, SLOT(abort()));

  progress->setVisible(false);
  button->setVisible(false);

  readOnly_ = false;

  // refresh state
  index_ = 0;
  cycle_ = simulator_->findAcceptingCycle();

  emit reset();
  updateActions();
}

bool SimulationProxy::isValidIndex(int state) const
{
  return isRunning() && simulator_->stateCount() > 0 && state < simulator_->stateCount();
}

void SimulationProxy::createActions()
{
  startAct_ = new QAction(MainForm::loadIcon(":/icons/sim/start"), tr("&Start"), this);
  startAct_->setObjectName("main.action.sim.start");
  startAct_->setShortcut(tr("F5"));
  startAct_->setStatusTip(tr("Start simulation"));
  connect(startAct_, SIGNAL(triggered(bool)), SLOT(start()));

  stopAct_ = new QAction(MainForm::loadIcon(":/icons/sim/stop"), tr("Sto&p"), this);
  stopAct_->setObjectName("main.action.sim.stop");
  stopAct_->setShortcut(tr("Shift+F5"));
  stopAct_->setStatusTip(tr("Stop the simulation"));
  connect(stopAct_, SIGNAL(triggered(bool)), SLOT(stop()));

  restartAct_ = new QAction(MainForm::loadIcon(":/icons/sim/restart"),
                            tr("&Restart"), this);
  restartAct_->setObjectName("main.action.sim.restart");
  restartAct_->setStatusTip(tr("Return to the initial state"));
  connect(restartAct_, SIGNAL(triggered(bool)), SLOT(restart()));

  prevAct_ = new QAction(MainForm::loadIcon(":/icons/sim/prev"), tr("Step B&ack"), this);
  prevAct_->setObjectName("main.action.sim.stepBack");
  prevAct_->setShortcut(tr("F6"));
  prevAct_->setStatusTip(tr("Move to previous state on stack"));
  connect(prevAct_, SIGNAL(triggered(bool)), SLOT(previous()));

  nextAct_ = new QAction(MainForm::loadIcon(":/icons/sim/next"),
                             tr("Step &Forward"), this);
  nextAct_->setObjectName("main.action.sim.stepForward");
  nextAct_->setShortcut(tr("F7"));
  nextAct_->setStatusTip(tr("Move to next state on stack"));
  connect(nextAct_, SIGNAL(triggered(bool)), SLOT(next()));

  randomAct_ = new QAction(MainForm::loadIcon(":/icons/sim/random"), tr("Ran&dom Step"), this);
  randomAct_->setObjectName("main.action.sim.stepRandom");
  randomAct_->setShortcut(tr("F8"));
  randomAct_->setStatusTip(tr("Choose and execute random transition"));
  connect(randomAct_, SIGNAL(triggered(bool)), SLOT(random()));

  randomRunAct_ = new QAction(tr("&Random Run..."), this);
  randomRunAct_->setObjectName("main.action.sim.randomRun");
  randomRunAct_->setShortcut(tr("Ctrl+F8"));
  connect(randomRunAct_, SIGNAL(triggered(bool)), SLOT(randomRun()));

  importAct_ = new QAction(MainForm::loadIcon(":/icons/sim/import"), tr("&Import..."), this);
  importAct_->setObjectName("main.action.sim.import");
  importAct_->setStatusTip(tr("Import simulation run"));
  connect(importAct_, SIGNAL(triggered(bool)), SLOT(importRun()));

  exportAct_ = new QAction(MainForm::loadIcon(":/icons/sim/export"), tr("&Export..."), this);
  exportAct_->setObjectName("main.action.sim.export");
  exportAct_->setStatusTip(tr("Export current simulation run"));
  connect(exportAct_, SIGNAL(triggered(bool)), SLOT(exportRun()));
}

void SimulationProxy::createMenus()
{
  QMenu * menu = root_->findChild<QMenu*>("main.menu.simulate");
  menu->addAction(startAct_);
  menu->addAction(prevAct_);
  menu->addAction(nextAct_);
  menu->addAction(randomAct_);
  menu->addAction(restartAct_);
  menu->addAction(stopAct_);
  QAction * sep = menu->addSeparator();
  sep->setObjectName("main.action.sim.separator1");
  menu->addAction(randomRunAct_);
  sep = menu->addSeparator();
  sep->setObjectName("main.action.sim.separator2");
  menu->addAction(importAct_);
  menu->addAction(exportAct_);
}

void SimulationProxy::createToolbars()
{
  QToolBar * toolbar = root_->addToolBar(tr("Simulate"));
  toolbar->setObjectName("main.toolbar.simulate");
  toolbar->addAction(startAct_);
  toolbar->addAction(prevAct_);
  toolbar->addAction(nextAct_);
  toolbar->addAction(randomAct_);
  toolbar->addAction(restartAct_);
  toolbar->addAction(stopAct_);

  QAction * action = toolbar->toggleViewAction();
  action->setText(tr("&Simulation"));
  action->setStatusTip(tr("Toggles the simulator toolbar"));

  QMenu * menu = root_->findChild<QMenu*>("main.menu.view.toolbars");
  menu->addAction(action);
}

}
}
