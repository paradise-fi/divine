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

#include "transitionDock.h"
#include "mainForm.h"
#include "simulationProxy.h"

namespace divine {
namespace gui {

TransitionDock::TransitionDock(MainForm * root)
    : QDockWidget(tr("Enabled Transitions"), root), sim_(root->simulation())
{
  tree_ = new QTreeWidget(this);
  tree_->setColumnCount(1);
  tree_->setHeaderHidden(true);
  tree_->setRootIsDecorated(false);
  tree_->setItemsExpandable(false);
  tree_->setMouseTracking(true);
  tree_->setAlternatingRowColors(true);

  setWidget(tree_);

  connect(tree_, SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)),
          SLOT(onCurrentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)));
  connect(tree_, SIGNAL(itemActivated(QTreeWidgetItem*, int)),
          SLOT(onItemActivated(QTreeWidgetItem*, int)));
  connect(tree_, SIGNAL(itemEntered(QTreeWidgetItem*, int)),
          SLOT(onItemEntered(QTreeWidgetItem*, int)));
  connect(tree_, SIGNAL(viewportEntered()), SLOT(onViewportEntered()));
  
  connect(sim_, SIGNAL(reset()), SLOT(updateTransitions()));
  connect(sim_, SIGNAL(currentIndexChanged(int)), SLOT(updateTransitions()));
  connect(sim_, SIGNAL(stateChanged(int)), SLOT(onStateChanged(int)));
  
  connect(this, SIGNAL(activateTransition(int)), sim_, SLOT(step(int)));
  connect(this, SIGNAL(highlightTransition(int)), sim_, SIGNAL(highlightTransition(int)));
}

void TransitionDock::updateTransitions()
{
  Q_ASSERT(sim_);

  tree_->clear();
  for (int i = 0; i < sim_->transitionCount(); ++i) {
    const Transition * trans = sim_->transition(i);
    Q_ASSERT(trans);
    
    QTreeWidgetItem * group = new QTreeWidgetItem(tree_);
    
    group->setText(0, trans->description);
  }

  tree_->resizeColumnToContents(0);
}

//! When mouse leaves this window, we must clear our highlighted transitions.
void TransitionDock::leaveEvent(QEvent * event)
{
  // reset the highlighted transition
  emit highlightTransition(-1);
}

void TransitionDock::onStateChanged(int index)
{
  if(index != sim_->currentIndex())
    return;
  
  updateTransitions();
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

  emit activateTransition(tree_->indexOfTopLevelItem(item));
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

void TransitionDock::onViewportEntered()
{
  emit highlightTransition(-1);
}

}
}
