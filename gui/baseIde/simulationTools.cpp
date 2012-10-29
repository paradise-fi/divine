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
#include <QMessageBox>

#include "mainForm.h"
#include "simulationProxy.h"
#include "simulationTools.h"

namespace divine {
namespace gui {

SimulationLock::SimulationLock(MainForm * root, QObject * parent)
  : QObject(parent), root_(root), doc_(NULL)
{
}

SimulationLock::~SimulationLock()
{
}

bool SimulationLock::isLocked(AbstractDocument * document) const
{
  return doc_ == document;
}

void SimulationLock::lock(AbstractDocument * document)
{
  Q_ASSERT(!doc_);
  Q_ASSERT(!root_->isLocked());

  doc_ = document;
  root_->lock(this);
}

bool SimulationLock::maybeRelease()
{
  if(!doc_)
    return true;

  // let's see if the simulation is really running :)
  Q_ASSERT(root_->simulation()->isRunning());

  int ret = QMessageBox::warning(root_, tr("DiVinE IDE"),
                                 tr("A simulation is running.\n"
                                    "Do you want to abort it?"),
                                 QMessageBox::Yes | QMessageBox::Default,
                                 QMessageBox::No | QMessageBox::Escape);

  if (ret != QMessageBox::Yes)
    return false;

  // this should call forceRelease at the end
  root_->simulation()->stop();
  return true;
}

void SimulationLock::forceRelease()
{
  Q_ASSERT(doc_);
  Q_ASSERT(root_->isLocked(this));

  doc_ = NULL;
  root_->release(this);
}

SimulationLoader::SimulationLoader(QObject * parent)
  : QThread(parent), delay_(0), running_(false)
{
}

void SimulationLoader::run()
{
  running_ = true;
  current_ = 0;

  while(running_ && current_ < steps_.size()) {
    emit step(steps_[current_]);
    emit progress(++current_);
    msleep(delay_);
  }
  running_ = false;
}

void SimulationLoader::init(const QVector<int>& steps, int delay)
{
  steps_ = steps;
  delay_ = delay;
}

void SimulationLoader::abort()
{
  if(!running_)
    return;

  running_ = false;
}

}
}
