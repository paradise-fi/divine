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

#include "simulator.h"

SimulatorProxy::SimulatorProxy(Simulator * sim, QObject * parent)
    : QObject(parent), sim_(sim), lock_(NULL)
{
  Q_ASSERT(sim);

  sim->setParent(this);
}

SimulatorProxy::~SimulatorProxy()
{
}

bool SimulatorProxy::lock(QObject * owner)
{
  if (!sim_ || lock_)
    return false;

  lock_ = owner;

  return true;
}

bool SimulatorProxy::release(QObject * owner)
{
  if (!sim_ || !lock_)
    return false;

  lock_ = NULL;

  return true;
}

void SimulatorProxy::setChannelValue
(int cid, const QVariantList & val, QObject * owner)
{
  if (sim_ && (!lock_ || lock_ == owner))
    sim_->setChannelValue(cid, val);
}

void SimulatorProxy::setProcessState(int pid, int val, QObject * owner)
{
  if (sim_ && (!lock_ || lock_ == owner))
    sim_->setProcessState(pid, val);
}

void SimulatorProxy::setVariableValue
(int pid, int vid, const QVariant & val, QObject * owner)
{
  if (sim_ && (!lock_ || lock_ == owner))
    sim_->setVariableValue(pid, vid, val);
}

void SimulatorProxy::start(void)
{
  sim_->start();
}

void SimulatorProxy::restart(void)
{
  sim_->restart();
}

void SimulatorProxy::stop(void)
{
  sim_->stop();
}

void SimulatorProxy::undo(QObject * owner)
{
  if (sim_ && (!lock_ || lock_ == owner))
    sim_->undo();
}

void SimulatorProxy::redo(QObject * owner)
{
  if (sim_ && (!lock_ || lock_ == owner))
    sim_->redo();
}

void SimulatorProxy::step(int id, QObject * owner)
{
  if (sim_ && (!lock_ || lock_ == owner))
    sim_->step(id);
}

void SimulatorProxy::backtrace(int depth, QObject * owner)
{
  if (sim_ && (!lock_ || lock_ == owner))
    sim_->backtrace(depth);
}
