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

#include <QTreeWidget>

#include "transitions.h"
#include "simulator.h"

TransitionDock::TransitionDock(QWidget * parent)
    : QDockWidget(tr("Enabled Transitions"), parent)
{
  tree_ = new QTreeWidget(this);
  tree_->setObjectName("TransitionTree");
  tree_->setColumnCount(1);
  tree_->setHeaderHidden(true);
  tree_->setRootIsDecorated(false);
  tree_->setItemsExpandable(false);
  tree_->setMouseTracking(true);

  setWidget(tree_);

  connect(tree_, SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)),
          SLOT(onCurrentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)));
  connect(tree_, SIGNAL(itemActivated(QTreeWidgetItem*, int)),
          SLOT(onItemActivated(QTreeWidgetItem*, int)));
  connect(tree_, SIGNAL(itemEntered(QTreeWidgetItem*, int)),
          SLOT(onItemEntered(QTreeWidgetItem*, int)));
}

//! Update current simulator.
void TransitionDock::setSimulator(SimulatorProxy * sim)
{
  if (sim_ == sim)
    return;

  sim_ = sim;

  if (sim_) {
    connect(sim_, SIGNAL(started()), SLOT(updateTransitions()));
    connect(sim_, SIGNAL(stateChanged()), SLOT(updateTransitions()));
  } else {
    tree_->clear();
  }
}

//! When mouse leaves this window, we must clear our highlighted transitions.
void TransitionDock::leaveEvent(QEvent * event)
{
  // reset the highlighted transition
  emit highlightTransition(-1);
}

void TransitionDock::updateTransitions(void)
{
  Q_ASSERT(sim_);

  tree_->clear();

  const Simulator * simulator = sim_->simulator();

  for (int i = 0; i < simulator->transitionCount(); ++i) {
    QTreeWidgetItem * group = new QTreeWidgetItem(tree_);
    group->setText(0, simulator->transitionName(i));

    QStringList text = simulator->transitionText(i);
    foreach(QString str, text) {
      QTreeWidgetItem * item = new QTreeWidgetItem(group);
      item->setText(0, str);
    }

    group->setExpanded(true);
  }

  tree_->resizeColumnToContents(0);
}

void TransitionDock::onCurrentItemChanged(QTreeWidgetItem * current, QTreeWidgetItem *)
{
  if (!sim_ || !current)
    return;

  QTreeWidgetItem * item = current;

  // find toplevel item
  while (item->parent())
    item = item->parent();

  Q_ASSERT(tree_->indexOfTopLevelItem(item) != -1);

  emit highlightTransition(tree_->indexOfTopLevelItem(item));
}

void TransitionDock::onItemActivated(QTreeWidgetItem * item, int)
{
  if (!sim_)
    return;

  // find toplevel item
  while (item->parent())
    item = item->parent();

  Q_ASSERT(tree_->indexOfTopLevelItem(item) != -1);

  sim_->step(tree_->indexOfTopLevelItem(item));
}

void TransitionDock::onItemEntered(QTreeWidgetItem * item, int)
{
  if (!sim_)
    return;

  // find toplevel item
  while (item->parent())
    item = item->parent();

  Q_ASSERT(tree_->indexOfTopLevelItem(item) != -1);

  emit highlightTransition(tree_->indexOfTopLevelItem(item));
}
