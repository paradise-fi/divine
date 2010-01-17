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
#include <QTreeWidgetItem>
#include <QStyledItemDelegate>
#include <QHeaderView>
#include <QComboBox>
#include <QLineEdit>

#include "watch.h"
#include "simulator.h"
#include "print.h"

namespace
{

  class ReadOnlyDelegate : public QStyledItemDelegate
  {
    public:
      ReadOnlyDelegate(QObject * parent = NULL) : QStyledItemDelegate(parent) {}

      QWidget * createEditor(QWidget * parent, const QStyleOptionViewItem & option,
                             const QModelIndex & index) const {
        return NULL;
      }
  };

  class WatchDelegate : public QStyledItemDelegate
  {
    public:
      WatchDelegate(QObject * parent = NULL) : QStyledItemDelegate(parent) {}

      QWidget * createEditor(QWidget * parent, const QStyleOptionViewItem & option,
                             const QModelIndex & index) const {
        if (index.column() != 1)
          return NULL;

        if (index.parent() == QModelIndex()) {
          // process states
          return new QComboBox(parent);
        } else {
          return QStyledItemDelegate::createEditor(parent, option, index);
        }
      }

      void setEditorData(QWidget * editor, const QModelIndex & index) const {
        if (index.parent() == QModelIndex()) {
          QComboBox * combo = qobject_cast<QComboBox*>(editor);

          Q_ASSERT(combo);

          const QStringList items = index.model()->data(index, Qt::UserRole).toStringList();
          combo->addItems(items);
          combo->setCurrentIndex(items.indexOf(index.model()->data(index, Qt::DisplayRole).toString()));
        } else if (index.model()->data(index, Qt::DisplayRole).type() == QVariant::String) {
          QLineEdit * line = qobject_cast<QLineEdit*>(editor);

          Q_ASSERT(line);

          line->setText(unquoteString(index.model()->data(index, Qt::DisplayRole).toString()));
        } else {
          return QStyledItemDelegate::setEditorData(editor, index);
        }
      }

      void setModelData(QWidget * editor, QAbstractItemModel * model, const QModelIndex & index) const {
        if (index.parent() == QModelIndex()) {
          QComboBox * combo = qobject_cast<QComboBox*>(editor);

          Q_ASSERT(combo);

          model->setData(index, combo->currentText());
        } else if (index.model()->data(index, Qt::DisplayRole).type() == QVariant::String) {
          QLineEdit * line = qobject_cast<QLineEdit*>(editor);

          Q_ASSERT(line);

          model->setData(index, quoteString(line->text()));
        } else {
          return QStyledItemDelegate::setModelData(editor, model, index);
        }
      }
  };

  class SignalLock
  {
    public:
      SignalLock(QObject * o) : obj_(o), prev_(o->signalsBlocked()) {
        obj_->blockSignals(true);
      }

      ~SignalLock() {obj_->blockSignals(prev_);}

    private:
      QObject * obj_;
      bool prev_;
  };

}

WatchDock::WatchDock(QWidget * parent) : QDockWidget(tr("Watch"), parent)
{
  tree_ = new QTreeWidget(this);
  tree_->setObjectName("WatchTree");
  tree_->setColumnCount(2);
  tree_->header()->setMovable(false);
  tree_->headerItem()->setText(0, tr("Name"));
  tree_->headerItem()->setText(1, tr("Value"));

  setWidget(tree_);

  connect(tree_, SIGNAL(itemChanged(QTreeWidgetItem *, int)),
          SLOT(onItemChanged(QTreeWidgetItem *, int)));
}

/*!
 * Switches between read-only and read-write state of the widget.
 * \param state True if read-only.
 */
void WatchDock::setReadOnly(bool state)
{
  tree_->itemDelegate()->deleteLater();

  QAbstractItemDelegate * delegate;

  if (state)
    delegate = new ReadOnlyDelegate(this);
  else
    delegate = new WatchDelegate(this);

  tree_->setItemDelegate(delegate);
}

//! Update current simulator.
void WatchDock::setSimulator(SimulatorProxy * sim)
{
  if (sim_ == sim)
    return;

  sim_ = sim;

  if (sim_) {
    setReadOnly(sim_->isLocked());

    connect(sim_, SIGNAL(started()), SLOT(resetWatch()));
    connect(sim_, SIGNAL(stateChanged()), SLOT(updateWatch()));
    connect(sim_, SIGNAL(locked(bool)), SLOT(setReadOnly(bool)));
  } else {
    tree_->clear();
  }
}

void WatchDock::updateChannels(QTreeWidgetItem * group, bool editable)
{
  Q_ASSERT(group);
  Q_ASSERT(sim_);

  QTreeWidgetItem * item;
  QVariantList list;

  for (int i = 0; i < group->childCount(); ++i) {
    item = group->child(i);
    list = sim_->simulator()->channelValue(item->data(0, Qt::UserRole).toInt());
    updateArray(item, list, editable);
  }
}

// TODO: proper behaviour for complex structures
void WatchDock::updateProcess(QTreeWidgetItem * group, int pid, bool editable)
{
  Q_ASSERT(group);
  Q_ASSERT(sim_);

  QTreeWidgetItem * item;
  QVariant val;

  const Simulator * simulator = sim_->simulator();

  if (pid != Simulator::globalId) {
    // NOTE: Divine reports invalid state value for errorneous processes
    const int state = simulator->processState(pid);
    QStringList list = group->data(1, Qt::UserRole).toStringList();
    if (state >= 0 && state < list.size())
      group->setText(1, list.at(state));
    else
      group->setText(1, tr("-Error-"));
  }

  for (int i = 0; i < group->childCount(); ++i) {
    item = group->child(i);
    val = simulator->variableValue(pid, item->data(0, Qt::UserRole).toInt());

    // disable const vars
    const int flags = simulator->variableFlags(pid, item->data(0, Qt::UserRole).toInt());
    editable = editable && (flags & Simulator::varConst) == 0;

    if (val.type() == QVariant::List) {
      updateArray(item, val.toList(), editable);
    } else if (val.type() == QVariant::Map) {
      updateMap(item, val.toMap(), editable);
    } else {
      updateScalar(item, val, editable);
    }
  }
}

void WatchDock::updateArray
(QTreeWidgetItem * item, const QVariantList & array, bool editable)
{
  Q_ASSERT(item);

  SignalLock lock(tree_);

  QVariant val;

  // mark the item as an array
  item->setText(1, prettyPrint(array));
  item->setData(1, Qt::UserRole, (int)QVariant::List);

  // equalize items by removing last
  while (item->childCount() > array.size()) {
    item->removeChild(item->child(item->childCount() - 1));
  }

  // or adding at the end
  while (item->childCount() < array.size()) {
    QTreeWidgetItem * tmp = new QTreeWidgetItem(item, item->type() + 1);
    tmp->setText(0, QString("[%1]").arg(item->childCount() - 1));
  }

  // go through all the items
  for (int i = 0; i < item->childCount(); ++i) {
    val = array.at(i);

    if (val.type() == QVariant::List) {
      updateArray(item->child(i), val.toList(), editable);
    } else if (val.type() == QVariant::Map) {
      updateMap(item->child(i), val.toMap(), editable);
    } else {
      updateScalar(item->child(i), val, editable);
    }
  }
}

void WatchDock::updateMap(QTreeWidgetItem * item, const QVariantMap & map, bool editable)
{
  Q_ASSERT(item);

  SignalLock lock(tree_);

  QVariant val;

  // mark the item as a map
  item->setText(1, prettyPrint(map));
  item->setData(1, Qt::UserRole, (int)QVariant::Map);

  // equalize items by removing last
  while (item->childCount() > map.size()) {
    item->removeChild(item->child(item->childCount() - 1));
  }

  // or adding at the end
  while (item->childCount() < map.size()) {
    QTreeWidgetItem * tmp = new QTreeWidgetItem(item, item->type() + 1);
  }

  // go through all the items
  QVariantMap::const_iterator itr1 = map.begin();

  QTreeWidgetItemIterator itr2(item);

  for (; itr1 != map.end(); ++itr1, ++itr2) {
    (*itr2)->setText(0, itr1.key());

    if (itr1->type() == QVariant::List) {
      updateArray(*itr2, val.toList(), editable);
    } else if (itr1->type() == QVariant::Map) {
      updateMap(*itr2, val.toMap(), editable);
    } else {
      updateScalar(*itr2, val, editable);
    }
  }
}

void WatchDock::updateScalar
(QTreeWidgetItem * item, const QVariant & value, bool editable)
{
  Q_ASSERT(item);

  SignalLock lock(tree_);

  // set value and clear array mark
  if (value.type() == QVariant::String)
    item->setData(1, Qt::DisplayRole, prettyPrint(value));
  else
    item->setData(1, Qt::DisplayRole, value);

  item->setData(1, Qt::UserRole, 0);

  if (editable)
    item->setFlags(item->flags() | Qt::ItemIsEditable);
}

const QVariant WatchDock::packVariable(QTreeWidgetItem * item)
{
  // check array mark
  if (item->data(1, Qt::UserRole).toInt() == QVariant::List) {
    QVariantList list;

    for (int i = 0; i < item->childCount(); ++i) {
      list.append(packVariable(item->child(i)));
    }

    return QVariant(list);
  } else if (item->data(1, Qt::UserRole).toInt() == QVariant::Map) {
    QVariantMap map;

    for (int i = 0; i < item->childCount(); ++i) {
      map.insert(item->child(i)->text(0), packVariable(item->child(i)));
    }

    return QVariant(map);
  } else {
    if (item->data(1, Qt::DisplayRole).type() == QVariant::String)
      return unquoteString(item->data(1, Qt::DisplayRole).toString());
    else
      return item->data(1, Qt::DisplayRole);
  }
}

void WatchDock::resetWatch(void)
{
  Q_ASSERT(sim_);

  SignalLock lock(tree_);

  // item type stands for depth
  QTreeWidgetItem * group;
  QTreeWidgetItem * item;

  tree_->clear();

  const Simulator * simulator = sim_->simulator();

  // channels
  if (simulator->channelCount() > 0) {
    group = new QTreeWidgetItem(tree_, 0);
    group->setText(0, tr("[channels]"));
    group->setData(0, Qt::UserRole, Simulator::invalidId);

    for (int i = 0; i < simulator->channelCount(); ++i) {
      item = new QTreeWidgetItem(group, 1);
      item->setText(0, simulator->channelName(i));
      item->setData(0, Qt::UserRole, i);
    }
  }

  // globals
  if (simulator->variableCount(Simulator::globalId) > 0) {
    group = new QTreeWidgetItem(tree_, 0);
    group->setText(0, tr("[globals]"));
    group->setData(0, Qt::UserRole, Simulator::globalId);

    for (int i = 0; i < simulator->variableCount(Simulator::globalId); ++i) {
      item = new QTreeWidgetItem(group, 1);
      item->setText(0, simulator->variableName(Simulator::globalId, i));
      item->setData(0, Qt::UserRole, i);
    }
  }

  // processes
  for (int pid = 0; pid < simulator->processCount(); ++pid) {
    group = new QTreeWidgetItem(tree_, 0);
    group->setText(0, simulator->processName(pid));
    group->setData(0, Qt::UserRole, pid);
    group->setData(1, Qt::UserRole, simulator->enumProcessStates(pid));

    if ((simulator->capabilities() & Simulator::capProcessMutable) != 0)
      group->setFlags(group->flags() | Qt::ItemIsEditable);

    for (int i = 0; i < simulator->variableCount(pid); ++i) {
      item = new QTreeWidgetItem(group, 1);
      item->setText(0, simulator->variableName(pid, i));
      item->setData(0, Qt::UserRole, i);
    }
  }

  updateWatch();
}

void WatchDock::updateWatch(void)
{
  Q_ASSERT(sim_);

  SignalLock lock(tree_);

  const Simulator * simulator = sim_->simulator();

  for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
    QTreeWidgetItem * group = tree_->topLevelItem(i);
    const int id = group->data(0, Qt::UserRole).toInt();

    if (id == Simulator::invalidId) {
      updateChannels(group, (simulator->capabilities() & Simulator::capChannelMutable) != 0);
    } else {
      updateProcess(group, id, (simulator->capabilities() & Simulator::capVariableMutable) != 0);
    }
  }
}

void WatchDock::onItemChanged(QTreeWidgetItem * item, int)
{
  Q_ASSERT(sim_);
  SignalLock lock(tree_);

  const Simulator * simulator = sim_->simulator();

  if (item->type() == 0) {
    const int pid = item->data(0, Qt::UserRole).toInt();

    if (pid >= 0)
      sim_->setProcessState(pid, item->data(1, Qt::UserRole).toStringList().indexOf(item->text(1)));

    return;
  }

  // find root of the variable item
  while (item->type() > 1 && item->parent())
    item = item->parent();

  Q_ASSERT(item->type() == 1);
  Q_ASSERT(item->parent());

  const int pid = item->parent()->data(0, Qt::UserRole).toInt();
  const int id = item->data(0, Qt::UserRole).toInt();

  if (pid == Simulator::invalidId) {
    sim_->setChannelValue(id, packVariable(item).toList());
  } else {
    sim_->setVariableValue(pid, id, packVariable(item));
  }
}
