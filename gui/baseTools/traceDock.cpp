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

#include <QListWidget>
#include <QPaintEvent>
#include <QPainter>

#include "traceDock.h"
#include "cycleBar.h"
#include "simulationProxy.h"
#include "abstractSimulator.h"
#include "settings.h"
#include "prettyPrint.h"
#include "mainForm.h"
#include "signalLock.h"

// defaults
#include "baseToolsPlugin.h"

namespace divine {
namespace gui {

//
// TraceWidget
//
TraceWidget::TraceWidget(QWidget * parent) : QListWidget(parent)
{
  cbar_ = new CycleBar(this, this);
  lbar_ = new LineNumberBar(this, this);

  connect(this, SIGNAL(lineCountChanged()), lbar_, SLOT(lineCountChanged()));
  connect(lbar_, SIGNAL(widthChanged(int)), cbar_, SLOT(setMargin(int)));
  connect(lbar_, SIGNAL(widthChanged(int)), this, SLOT(updateMargins(int)));
}

const QPair<int, int> & TraceWidget::acceptingCycle() const
{
  return cbar_->cycle();
}

int TraceWidget::lineCount()
{
  return count();
}

int TraceWidget::firstVisibleLine(const QRect & rect)
{
  QListWidgetItem * item = itemAt(0, 0);
  return item ? row(item) : 0;
}

QList<int> TraceWidget::visibleLines(const QRect & rect)
{
  QList<int> lines;

  for(int i = firstVisibleLine(rect); i < count(); ++i) {
    QRect rct = visualItemRect(item(i));

    if(rct.top() >= rect.bottom())
      return lines;

    lines.append(rct.top());
  }
  return lines;
}

//! Set the displayed accepting cycle.
void TraceWidget::setAcceptingCycle(const QPair<int, int> & cycle)
{
  cbar_->setCycle(cycle);
}

void TraceWidget::updateMargins(int width)
{
  if (isLeftToRight())
    setViewportMargins(width + cbar_->width(), 0, 0, 0);
  else
    setViewportMargins(0, 0, width + cbar_->width(), 0);
}

//! Reimplements QListView::resizeEvent.
void TraceWidget::resizeEvent(QResizeEvent * event)
{
  QListWidget::resizeEvent(event);

  lbar_->updateGeometry();
  cbar_->updateGeometry();
}

//! Reimplements QAbstractScrollArea::scrollContentsBy.
void TraceWidget::scrollContentsBy(int dx, int dy)
{
  QListWidget::scrollContentsBy(dx, dy);

  lbar_->update();
  cbar_->update();
}

//
// TraceDock
//
TraceDock::TraceDock(MainForm * root)
    : QDockWidget(tr("Stack trace"), root), sim_(root->simulation()), state_(-1)
{
  trace_ = new TraceWidget(this);
  setWidget(trace_);

  readSettings();

  connect(trace_, SIGNAL(itemActivated(QListWidgetItem*)),
          SLOT(onItemActivated(QListWidgetItem*)));

  connect(root, SIGNAL(settingsChanged()), SLOT(readSettings()));

  connect(sim_, SIGNAL(reset()), SLOT(reset()));
  connect(sim_, SIGNAL(currentIndexChanged(int)), SLOT(setCurrentState(int)));
  connect(sim_, SIGNAL(stateChanged(int)), SLOT(updateState(int)));
  connect(sim_, SIGNAL(acceptingCycleChanged(QPair<int,int>)), trace_, SLOT(setAcceptingCycle(QPair<int,int>)));
  connect(sim_, SIGNAL(stateAdded(int,int)), SLOT(addStates(int,int)));
  connect(sim_, SIGNAL(stateRemoved(int,int)), SLOT(removeStates(int,int)));

  connect(trace_, SIGNAL(itemActivated(QListWidgetItem*)), SLOT(onItemActivated(QListWidgetItem*)));

  // side bars
  connect(sim_, SIGNAL(stateAdded(int,int)), trace_, SIGNAL(lineCountChanged()));
  connect(sim_, SIGNAL(stateRemoved(int,int)), trace_, SIGNAL(lineCountChanged()));
  connect(sim_, SIGNAL(reset()), trace_, SIGNAL(lineCountChanged()));
}

//! Reload settings.
void TraceDock::readSettings()
{
  Settings s("common.trace");

  QColor colorD = QColor(s.value("deadlock", defTraceDeadlock).value<QColor>());
  QColor colorE = QColor(s.value("error", defTraceError).value<QColor>());

  bool changed = colorD != deadlock_ || colorE != error_;

  deadlock_ = colorD;
  error_ = colorE;

  // update
  if(changed) {
    QPalette def;

    for(int i=0; i < trace_->count(); ++i) {
      QListWidgetItem * item = trace_->item(i);

      if (!sim_->isValid(i)) {
        item->setForeground(error_);
      } else if (sim_->transitionCount(i) == 0) {
        item->setForeground(deadlock_);
      } else {
        item->setForeground(def.color(QPalette::Text));
      }
    }
  }
}

void TraceDock::reset()
{
  SignalLock lock(this);

  trace_->clear();

  for(int i=0; i < sim_->stateCount(); ++i) {
    QListWidgetItem * item = new QListWidgetItem(trace_);
    updateState(i);
  }

  state_ = -1;
  setCurrentState(sim_->currentIndex());

  trace_->setAcceptingCycle(sim_->acceptingCycle());
}

void TraceDock::setCurrentState(int index)
{
  // nothing to do
  if(index == state_)
    return;

  // update previous
  QListWidgetItem * item = trace_->item(state_);
  if(item) {
    QFont fnt = item->font();
    fnt.setWeight(QFont::Normal);
    item->setFont(fnt);
  }

  // update new current
  item = trace_->item(index);
  if(item) {
    QFont fnt = item->font();
    fnt.setWeight(QFont::Bold);
    item->setFont(fnt);
  }

  state_ = index;

  trace_->setCurrentRow(index);
  trace_->scrollToItem(trace_->item(index));
}

void TraceDock::updateState(int index)
{
  QListWidgetItem * item = trace_->item(index);
  if(!item)
    return;

  item->setText(printChildren(sim_->topLevelSymbols(index), index));

  // set font
  QFont fnt = item->font();
  fnt.setItalic(sim_->isAccepting(index));
  item->setFont(fnt);

  // and background colour
  QPalette def;

  if (!sim_->isValid(index)) {
    item->setForeground(error_);
  } else if (sim_->transitionCount(index) == 0) {
    item->setForeground(deadlock_);
  } else {
    item->setForeground(def.color(QPalette::Text));
  }
}

void TraceDock::addStates(int from, int to)
{
  // sanity check
  from = qMax(0, from);
  to = qMin(to, sim_->stateCount());

  // overlap
  for(; from <= qMin(trace_->count() - 1, to); ++from) {
    updateState(from);
  }

  // new states
  for(; from <= to; ++from) {
    QListWidgetItem * item = new QListWidgetItem(trace_);
    updateState(from);
  }
}

void TraceDock::removeStates(int from, int to)
{
  // sanity check
  from = qMax(0, from);
  to = qMin(to, trace_->count());
  int count = to - from + 1;

  for(int i=0; i < count; ++i) {
    delete trace_->takeItem(from);
  }
}

QString TraceDock::printSymbol(const Symbol * symbol, int state)
{
  const SymbolType * type = sim_->typeInfo(symbol->type);
  if(!type)
    return QString();

  QString val1, val2, value;

  if(dynamic_cast<const PODType*>(type) != NULL) {
    val1 = printPODItem(sim_->symbolValue(symbol->id, state), dynamic_cast<const PODType*>(type));
  } else if(dynamic_cast<const OptionType*>(type) != NULL) {
    val1 = printOptionItem(sim_->symbolValue(symbol->id, state), dynamic_cast<const OptionType*>(type));
  } else if(dynamic_cast<const ArrayType*>(type) != NULL) {
    val1 = printArrayItem(sim_->symbolValue(symbol->id, state), dynamic_cast<const ArrayType*>(type), state);
  }

  val2 = printChildren(symbol->children, state);

  if(!val1.isEmpty() && val2 != "{}") {
    value = QString("{%1, %2}").arg(val1, val2);
  } else {
    value = !val1.isEmpty() ? val1 : val2;
  }
  return QString("%1: %2").arg(symbol->name, value);
}

QString TraceDock::printPODItem(const QVariant & value, const PODType * type)
{
  return value.toString();
}

QString TraceDock::printOptionItem(const QVariant & value, const OptionType * type)
{
  int index = value.toInt();

  if(index >= 0 && index < type->options.size())
    return type->options[index];

  return QString();
}

QString TraceDock::printArrayItem(const QVariant & value, const ArrayType * type, int state)
{
  QStringList caps;
  QVariantList array = value.value<QVariantList>();

  // incompatible arrays might lead to wrong indices and crashes
  if(array.size() != type->size)
    return QString();

  const SymbolType * subtype = sim_->typeInfo(type->subtype);

  // first check existing fields
  for(int i=0; i < type->size; ++i) {
    if(dynamic_cast<const PODType*>(subtype) != NULL) {
      caps << printPODItem(array[i], dynamic_cast<const PODType*>(subtype));
    } else if(dynamic_cast<const OptionType*>(subtype) != NULL) {
      caps << printOptionItem(array[i], dynamic_cast<const OptionType*>(subtype));
    } else if(dynamic_cast<const ArrayType*>(subtype) != NULL) {
      caps << printArrayItem(array[i], dynamic_cast<const ArrayType*>(subtype), state);
    } else if(dynamic_cast<const ListType*>(subtype) != NULL) {
      caps << printChildren(array[i].toStringList(), state);
    } else {
      caps << "";
    }
  }
  return QString("{%1}").arg(caps.join(","));
}

QString TraceDock::printChildren(const QStringList & children, int state)
{
  QStringList caps;
  foreach(QString sym, children) {
    const Symbol * symbol = sim_->symbol(sym, state);

    if(!symbol)
      continue;

    QString cap = printSymbol(symbol, state);
    if(!cap.isEmpty())
      caps << cap;
  }
  return QString("{%1}").arg(caps.join(", "));
}

void TraceDock::onItemActivated(QListWidgetItem * item)
{
  sim_->setCurrentIndex(trace_->row(item));
}

}
}
