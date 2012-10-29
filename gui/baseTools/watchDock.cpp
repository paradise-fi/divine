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
#include <QSpinBox>
#include <QDoubleSpinBox>

#include "watchDock.h"
#include "mainForm.h"
#include "simulationProxy.h"
#include "prettyPrint.h"
#include "signalLock.h"

namespace divine {
namespace gui {
  
namespace {

  class WatchDelegate : public QStyledItemDelegate
  {
    public:
      WatchDelegate(QObject * parent, SimulationProxy * sim)
        : QStyledItemDelegate(parent), sim_(sim) {}

      QWidget * createEditor(QWidget * parent, const QStyleOptionViewItem & option,
                             const QModelIndex & index) const {
        if(sim_->isReadOnly() || index.column() != 1)
          return NULL;

        QString id = index.model()->data(index, Qt::UserRole).toString();
        const SymbolType * type = sim_->typeInfo(id);
        if(dynamic_cast<const OptionType*>(type)) {
          return new QComboBox(parent);
        } else {
          return QStyledItemDelegate::createEditor(parent, option, index);
        }
      }

      void setEditorData(QWidget * editor, const QModelIndex & index) const {
        QString id = index.model()->data(index, Qt::UserRole).toString();
        const SymbolType * type = sim_->typeInfo(id);
        
        // option type
        if(dynamic_cast<const OptionType*>(type)) {
          QComboBox * combo = qobject_cast<QComboBox*>(editor);
          Q_ASSERT(combo);
          
          const QStringList items = index.model()->data(index, Qt::UserRole + 2).toStringList();
          combo->addItems(items);
          combo->setCurrentIndex(index.model()->data(index, Qt::UserRole + 1).toInt());
        // pod type
        } else if(dynamic_cast<const PODType*>(type)) {
          QLineEdit * line = qobject_cast<QLineEdit*>(editor);
          
          if(line) {
            line->setText(index.model()->data(index, Qt::UserRole + 1).toString());
          } else {
            return QStyledItemDelegate::setEditorData(editor, index);
          }
        }
      }

      void setModelData(QWidget * editor, QAbstractItemModel * model, const QModelIndex & index) const {
        QString id = index.model()->data(index, Qt::UserRole).toString();
        const SymbolType * type = sim_->typeInfo(id);
        
        // option type
        if(dynamic_cast<const OptionType*>(type)) {
          QComboBox * combo = qobject_cast<QComboBox*>(editor);
          Q_ASSERT(combo);
          
          model->setData(index, combo->currentText(), Qt::DisplayRole);
          model->setData(index, combo->currentIndex(), Qt::UserRole + 1);
        // pod type
        } else if(dynamic_cast<const PODType*>(type)) {
          QLineEdit * line;
          QSpinBox * sbox;
          QDoubleSpinBox * ssbox;
        
          if(line = qobject_cast<QLineEdit*>(editor)) {
            model->setData(index, quoteString(line->text()), Qt::DisplayRole);
            model->setData(index, line->text(), Qt::UserRole + 1);
          } else if(sbox = qobject_cast<QSpinBox*>(editor)) {
            model->setData(index, sbox->value(), Qt::DisplayRole);
            model->setData(index, sbox->value(), Qt::UserRole + 1);
          } else if(ssbox = qobject_cast<QDoubleSpinBox*>(editor)) {
            model->setData(index, ssbox->value(), Qt::DisplayRole);
            model->setData(index, ssbox->value(), Qt::UserRole + 1);
          }
        }
      }
    private:
      SimulationProxy * sim_;
  };

}

WatchDock::WatchDock(MainForm * root)
  : QDockWidget(tr("Watch"), root), sim_(root->simulation())
{
  tree_ = new QTreeWidget(this);
  tree_->setObjectName("common.dock.watch.tree");
  tree_->setColumnCount(2);
  tree_->header()->setMovable(false);
  tree_->headerItem()->setText(0, tr("Name"));
  tree_->headerItem()->setText(1, tr("Value"));

  tree_->setItemDelegate(new WatchDelegate(this, sim_));
  
  setWidget(tree_);
  
  connect(sim_, SIGNAL(reset()), SLOT(updateWatch()));
  connect(sim_, SIGNAL(currentIndexChanged(int)), SLOT(updateWatch()));
  connect(sim_, SIGNAL(stateChanged(int)), SLOT(onStateChanged(int)));

  connect(tree_, SIGNAL(itemChanged(QTreeWidgetItem*, int)), SLOT(onItemChanged(QTreeWidgetItem*,int)));
}

void WatchDock::updateWatch()
{
  Q_ASSERT(sim_);

  SignalLock lock(tree_);
  
  updateChildren(NULL, sim_->topLevelSymbols());
}

void WatchDock::updateSymbol(QTreeWidgetItem * item, const Symbol * symbol)
{
  item->setData(0, Qt::DisplayRole, symbol->name);
  item->setData(0, Qt::UserRole, symbol->id);
  item->setData(0, Qt::DecorationRole, symbol->icon);
  
  const SymbolType * type = sim_->typeInfo(symbol->type);
  if(!type)
    return;
  
  item->setData(0, Qt::ToolTipRole, type->name);
  
  if(dynamic_cast<const PODType*>(type) != NULL) {
    updatePODItem(item, sim_->symbolValue(symbol->id),
                 dynamic_cast<const PODType*>(type), symbol->hasFlag(Symbol::ReadOnly));
  } else if(dynamic_cast<const OptionType*>(type) != NULL) {
    updateOptionItem(item, sim_->symbolValue(symbol->id),
                    dynamic_cast<const OptionType*>(type), symbol->hasFlag(Symbol::ReadOnly));
  } else if(dynamic_cast<const ArrayType*>(type) != NULL) {
    updateArrayItem(item, sim_->symbolValue(symbol->id),
                   dynamic_cast<const ArrayType*>(type), symbol->hasFlag(Symbol::ReadOnly));
  }

  updateChildren(item, symbol->children);
  
  // update list's caption
  if(dynamic_cast<const ListType*>(type) != NULL) {
    updateListCaption(item);
  }
}

void WatchDock::updatePODItem(QTreeWidgetItem * item, const QVariant & value,
                              const PODType * type, bool readOnly)
{
  item->setFlags(!readOnly ? item->flags() | Qt::ItemIsEditable : item->flags());
  
  item->setData(1, Qt::DisplayRole, value.type() == QVariant::String ?
    quoteString(value.toString()) : value);
  item->setData(1, Qt::UserRole, type->id);
  item->setData(1, Qt::UserRole + 1, value);
}

void WatchDock::updateOptionItem(QTreeWidgetItem * item, const QVariant & value,
                                 const OptionType * type, bool readOnly)
{
  item->setFlags(!readOnly ? item->flags() | Qt::ItemIsEditable : item->flags());
  
  int index = value.toInt();
  
  if(index >= 0 && index < type->options.size())
    item->setData(1, Qt::DisplayRole, type->options[index]);
  
  item->setData(1, Qt::UserRole, type->id);
  item->setData(1, Qt::UserRole + 1, index);
  item->setData(1, Qt::UserRole + 2, type->options);
}

void WatchDock::updateArrayItem(QTreeWidgetItem * item, const QVariant & value,
                                const ArrayType * type, bool readOnly)
{
  QStringList caps;
  QVariantList array = value.value<QVariantList>();
  
  item->setData(1, Qt::UserRole, type->id);
  
  // incompatible arrays might lead to wrong indices and crashes
  if(array.size() != type->size)
    return;
  
  const SymbolType * subtype = sim_->typeInfo(type->subtype);
  
  // first check existing fields
  for(int i=0; i < type->size; ++i) {
    QTreeWidgetItem * subitem;
    
    // reuse or create
    if(i >= item->childCount())
      subitem = new QTreeWidgetItem(item);
    else
      subitem = item->child(i);
    
    subitem->setData(0, Qt::DisplayRole, QString("[%1]").arg(i));
    
    if(dynamic_cast<const PODType*>(subtype) != NULL) {
      updatePODItem(subitem, array[i], dynamic_cast<const PODType*>(subtype), readOnly);
    } else if(dynamic_cast<const OptionType*>(subtype) != NULL) {
      updateOptionItem(subitem, array[i], dynamic_cast<const OptionType*>(subtype), readOnly);
    } else if(dynamic_cast<const ArrayType*>(subtype) != NULL) {
      updateArrayItem(subitem, array[i], dynamic_cast<const ArrayType*>(subtype), readOnly);
    } else if(dynamic_cast<const ListType*>(subtype) != NULL) {
      updateListItem(subitem, array[i], dynamic_cast<const ListType*>(subtype));
    }
    caps.append(subitem->data(1, Qt::DisplayRole).toString());
  }
  // finish the name
  item->setData(1, Qt::DisplayRole, QString("{%1}").arg(caps.join(",")));
  
  // trim excess children (array children don't have Symbol UUID)
  while(item->childCount() > type->size && item->data(0, Qt::UserRole).isNull())
    delete item->takeChild(item->childCount() - 1);
}

void WatchDock::updateListItem(QTreeWidgetItem * item, const QVariant & value, const ListType * type)
{
  item->setData(1, Qt::UserRole, type->id);
  
  updateChildren(item, value.toStringList());
  updateListCaption(item);
}

void WatchDock::updateChildren(QTreeWidgetItem * parent, QStringList children)
{
  // walk through all existing items, update and remove from child list
  int count = parent ? parent->childCount() : tree_->topLevelItemCount();
  for(int i=0; i < count; ++i) {
    QTreeWidgetItem * item = parent ? parent->child(i) : tree_->topLevelItem(i);
 
    // All symbols have UUID in the [0,UR]. This is also to mark the difference
    // between symbols and array items. This is necessary evil we have to live
    // if we want to keep the abstraction of the current type system
    if(item->data(0, Qt::UserRole).isNull())
      continue;
    
    QString id = item->data(0, Qt::UserRole).toString();
    
    // if not child anymore, remove it    
    if(!children.removeOne(id)) {
      delete item;
      --i, --count;
      continue;
    }
    
    const Symbol * symbol = sim_->symbol(id);
    if(!symbol)
      continue;
    
    updateSymbol(item, symbol);
  }
  
  // add remaining children
  foreach(QString id, children) {
    const Symbol * symbol = sim_->symbol(id);
    
    if(!symbol)
      continue;
    
    QTreeWidgetItem * item;
    if(parent)
      item = new QTreeWidgetItem(parent);
    else
      item = new QTreeWidgetItem(tree_);
      
    updateSymbol(item, symbol);
  }
}

void WatchDock::updateListCaption(QTreeWidgetItem * parent)
{
  QStringList caps;
  
  // list has only symbols
  for(int i=0; i < parent->childCount(); ++i) {
    QString caption = parent->child(i)->data(1, Qt::DisplayRole).toString();
    caps.append(caption);
  }
  parent->setData(1, Qt::DisplayRole, QString("{%1}").arg(caps.join(",")));
}

QVariant WatchDock::packItem(QTreeWidgetItem * item)
{
  QVariant packed;
  const SymbolType * type = sim_->typeInfo(item->data(1, Qt::UserRole).toString());
  
  // we are either a leaf - option or POD type
  // or an array, since lists have symbols as children
  if(dynamic_cast<const ArrayType*>(type)) {
    QVariantList array;
    
    for(int i=0; i < item->childCount(); ++i) {
      array.append(packItem(item->child(i)));
    }
    packed = array;
  } else if(dynamic_cast<const OptionType*>(type)) {
    packed = item->data(1, Qt::UserRole + 1);
  } else if(dynamic_cast<const PODType*>(type)) {
    packed = item->data(1, Qt::UserRole + 1);
  }
  return packed;
}

void WatchDock::onStateChanged(int index)
{
  if(index != sim_->currentIndex())
    return;
  
  updateWatch();
}

void WatchDock::onItemChanged(QTreeWidgetItem * item, int column)
{
  if(column != 1)
    return;
  
  // find parent symbol
  while(item && item->data(0, Qt::UserRole).toString().isEmpty())
    item = item->parent();
  
  Q_ASSERT(item);

  QString id = item->data(0, Qt::UserRole).toString();
  bool succ = sim_->setSymbolValue(id, packItem(item));
  
  // change refused (whatever the reason)
  if(!succ) {
    SignalLock lock_(tree_);
    
    const Symbol * symbol = sim_->symbol(id);
    if(symbol)
      updateSymbol(item, symbol);
  }
}

}
}
