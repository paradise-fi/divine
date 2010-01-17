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
  
  // forward signals
  connect(sim, SIGNAL(message(QString)), SIGNAL(message(QString)));
  connect(sim, SIGNAL(started()), SIGNAL(started()));
  connect(sim, SIGNAL(stopped()), SIGNAL(stopped()));
  connect(sim, SIGNAL(stateChanged()), SIGNAL(stateChanged()));
}

SimulatorProxy::~SimulatorProxy()
{
}

/*!
 * Locks the simulator owner.
 * \return Success of the operation.
 */
bool SimulatorProxy::lock(QObject * owner)
{
  if (!sim_ || lock_)
    return false;

  lock_ = owner;

  return true;
}

/*!
 * Releases lock on the simulator.
 * \param owner Caller, must be the same as the lock's owner.
 * \return Success of the operation.
 */
bool SimulatorProxy::release(QObject * owner)
{
  if (!sim_ || !lock_)
    return false;

  lock_ = NULL;

  return true;
}

/*!
 * Calls Simulator::setChannelValue.
 * \param cid Channel ID.
 * \param val New value.
 * \param owner Caller, must be the same as the lock's owner.
 */
void SimulatorProxy::setChannelValue
(int cid, const QVariantList & val, QObject * owner)
{
  if (sim_ && (!lock_ || lock_ == owner))
    sim_->setChannelValue(cid, val);
}

/*!
 * Calls Simulator::setProcessState.
 * \param pid Process ID.
 * \param val New state value.
 * \param owner Caller, must be the same as the lock's owner.
 */
void SimulatorProxy::setProcessState(int pid, int val, QObject * owner)
{
  if (sim_ && (!lock_ || lock_ == owner))
    sim_->setProcessState(pid, val);
}

/*!
 * Calls Simulator::setVariableValue.
 * \param pid Process ID.
 * \param vid Variable ID.
 * \param val New value.
 * \param owner Caller, must be the same as the lock's owner.
 */
void SimulatorProxy::setVariableValue
(int pid, int vid, const QVariant & val, QObject * owner)
{
  if (sim_ && (!lock_ || lock_ == owner))
    sim_->setVariableValue(pid, vid, val);
}

//! Calls Simulator::start.
void SimulatorProxy::start(void)
{
  sim_->start();
}

//! Calls Simulator::stop.
void SimulatorProxy::stop(void)
{
  sim_->stop();
}

/*!
 * Calls Simulator::undo.
 * \param owner Caller, must be the same as the lock's owner.
 */
void SimulatorProxy::undo(QObject * owner)
{
  if (sim_ && (!lock_ || lock_ == owner))
    sim_->undo();
}

/*!
 * Calls Simulator::redo.
 * \param owner Caller, must be the same as the lock's owner.
 */
void SimulatorProxy::redo(QObject * owner)
{
  if (sim_ && (!lock_ || lock_ == owner))
    sim_->redo();
}

/*!
 * Calls Simulator::step.
 * \param id Transition ID.
 * \param owner Caller, must be the same as the lock's owner.
 */
void SimulatorProxy::step(int id, QObject * owner)
{
  if (sim_ && (!lock_ || lock_ == owner))
    sim_->step(id);
}

/*!
 * Calls Simulator::backstep.
 * \param depth Desired stack position.
 * \param owner Caller, must be the same as the lock's owner.
 */
void SimulatorProxy::backstep(int depth, QObject * owner)
{
  if (sim_ && (!lock_ || lock_ == owner))
    sim_->backstep(depth);
}
