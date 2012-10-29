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

#include <QtGui>

#include <QHeaderView>
#include <QScrollBar>
#include <QPaintEvent>
#include <QPainter>

#include "mscDock.h"
#include "mainForm.h"
#include "simulationProxy.h"
#include "signalLock.h"

namespace divine {
namespace gui {

// TODO: write support for dynamic processes
  
static const int maxFinWidth = 40;

//
// MSCModel class
//
//! Creates the model for given simulator.
MSCModel::MSCModel(SimulationProxy * sim, QObject * parent)
    : QAbstractItemModel(parent), sim_(sim)
{
  Q_ASSERT(sim);

  connect(sim, SIGNAL(reset()), SLOT(onReset()));
  connect(sim, SIGNAL(currentIndexChanged(int)), SLOT(onCurrentIndexChanged(int)));
  connect(sim, SIGNAL(stateChanged(int)), SLOT(onStateChanged(int)));
  connect(sim, SIGNAL(stateAdded(int,int)), SLOT(onStateAdded(int,int)));
  connect(sim, SIGNAL(stateRemoved(int,int)), SLOT(onStateRemoved(int,int)));
  
  current_ = sim_->currentIndex();
  
  onReset();
}

//! Implements QAbstractItemModel::rowCount.
int MSCModel::rowCount(const QModelIndex & parent) const
{
  return sim_->stateCount();
}

//! Implements QAbstractItemModel::columnCount.
int MSCModel::columnCount(const QModelIndex & parent) const
{
  if(parent != QModelIndex())
    return 0;
  
  return procs_.count() + 1;  // the last column is padding
}

//! Reimplements QAbstractItemModel::headerData.
QVariant MSCModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (!sim_ || role != Qt::DisplayRole || orientation != Qt::Horizontal ||
      section < 0 || section >= procs_.count())
    return QVariant();

  const Symbol * sym = sim_->symbol(procs_[section].id);
  
  if(sym)
    return sym->name;
  
  return QVariant();
}

//! Implements QAbstractItemModel::data.
QVariant MSCModel::data(const QModelIndex & index, int role) const
{
  const int proc = index.column();
  const int state = index.row();
  
  if (!sim_ || !index.isValid() ||
      state < 0 || state >= sim_->stateCount() ||
      proc < 0 || proc >= procs_.count()) {
    return QVariant();
  }
  
  const Symbol * sym = sim_->symbol(procs_[proc].id, state);
  if(!sym)
    return QVariant();
  
  int value = sim_->symbolValue(sym->id, state).toInt();

  switch(role) {
    case Qt::DisplayRole:
      // value hasn't changed, break
      if(state > 0 && value == sim_->symbolValue(sym->id, state - 1).toInt())
        break;
    case stateRole:
      if(value >= 0 && value < procs_[proc].states.count())
        return procs_[proc].states[value];
      else
        return tr("-Err-");
    case currentStateRole:
      return sim_->currentIndex() == state;
    case enumStatesRole:
      return procs_[proc].states;
    case messageRole:
      return getMessage(state);
    case messageFromRole:
      return getMessageSender(state);
    case messageToRole:
      return getMessageRecipient(state);
  }
  return QVariant();
}

//! Implements QAbstractItemModel::index.
QModelIndex MSCModel::index(int row, int column, const QModelIndex & parent) const
{
  if (row < 0 || row >= rowCount() || column < 0 ||
      column >= columnCount() || parent != QModelIndex()) {
    return QModelIndex();
  }

  return createIndex(row, column);
}

//! Implements QAbstractItemModel::parent.
QModelIndex MSCModel::parent(const QModelIndex & index) const
{
  return QModelIndex();
}

void MSCModel::onReset()
{
  beginResetModel();
 
  procs_.clear();
  
  // queary the first state
  foreach(QString id, sim_->topLevelSymbols()) {
    const Symbol* smb = sim_->symbol(id, 0);
    
    if(!smb)
      continue;

    const OptionType* type = dynamic_cast<const OptionType*>(sim_->typeInfo(smb->type));
    
    if(!type)
      continue;

    ProcessInfo proc;

    proc.id = id;
    proc.origin = 0;
    proc.states = type->options;
    procs_.append(proc);
  }
  endResetModel();
}

void MSCModel::onCurrentIndexChanged(int index)
{
  if(current_ != -1)
    emit dataChanged(createIndex(index, 0), createIndex(index, columnCount() - 1));
  
  current_ = index;
  
  if(current_ != -1)
    emit dataChanged(createIndex(index, 0), createIndex(index, columnCount() - 1));
}
    
void MSCModel::onStateChanged(int index)
{
  Q_ASSERT(index >= 0 && index < sim_->stateCount());
  
  emit dataChanged(createIndex(index, 0), createIndex(index, columnCount() - 1));
}

void MSCModel::onStateAdded(int from, int to)
{
  beginInsertRows(QModelIndex(), from, to);
  endInsertRows();
}

void MSCModel::onStateRemoved(int from, int to)
{
  beginRemoveRows(QModelIndex(), from, to);
  endRemoveRows();
}

QVariant MSCModel::getMessage(int state) const
{
  int tid = sim_->usedTransition(state);

  if(tid == -1)
    return QVariant();
  
  QList<const Communication*> comms = sim_->communication(tid, state);
   
  if(comms.isEmpty())
    return QVariant();

  const Symbol * smb = sim_->symbol(comms.first()->medium, state);
  return smb ? smb->name : QVariant();
}

QVariant MSCModel::getMessageSender(int state) const
{
  int tid = sim_->usedTransition(state);
 
  if(tid == -1)
    return -1;
  
  QList<const Communication*> comms = sim_->communication(tid, state);
  
  if(comms.isEmpty())
    return -1;
  
  // find the process column
  for(int i=0; i < procs_.size(); ++i) {
    if(procs_[i].id == comms.first()->from)
      return i;
  }
  return -1;
}

QVariant MSCModel::getMessageRecipient(int state) const
{
  int tid = sim_->usedTransition(state);
  
  if(tid == -1)
    return -1;
  
  QList<const Communication*> comms = sim_->communication(tid, state);
  
  if(comms.isEmpty())
    return -1;
  
  // find the process column
  for(int i=0; i < procs_.size(); ++i) {
    if(procs_[i].id == comms.first()->to)
      return i;
  }
  return -1;
}

//
// MSCDelegate class
//
MSCDelegate::MSCDelegate(QObject * parent) : QAbstractItemDelegate(parent)
{
}

//! Implements QAbstractItemDelegate::paint.
void MSCDelegate::paint
(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const
{
  const QStyleOptionViewItemV3 * vopt;

  if (!(vopt = qstyleoption_cast<const QStyleOptionViewItemV3*>(&option)) || !index.isValid())
    return;

  const QColor dia_col(220, 220, 220, 128);

  QPen pen_fin;
  pen_fin.setWidth(3);

  const int middle = (vopt->rect.left() + vopt->rect.right()) / 2;
  const bool current = index.model()->data(index, MSCModel::currentStateRole).toBool();
  const bool details = (vopt->state & QStyle::State_Selected) ||
                 (vopt->state & QStyle::State_MouseOver) || current;

  const int row_hint = sizeHint(option, index).height();

  const QStyle * style = vopt->widget ? vopt->widget->style() : QApplication::style();
  style->drawPrimitive(QStyle::PE_PanelItemViewItem, vopt, painter, vopt->widget);

  // leave the last column blank
  if (index.column() == index.model()->columnCount() - 1)
    return;

  // draw diagram
  const QVariant var = index.model()->data(index, details ? MSCModel::stateRole : Qt::DisplayRole);

  int fin_width = qMin(maxFinWidth / 2, (int)(option.rect.width() * 0.3));
  int fin_y = vopt->rect.bottom() - row_hint / 10;
  
  if (var.isValid()) {
    QString text = var.toString();

    const int fw = vopt->fontMetrics.width(text) + 8;
    QRect frame = QRect(vopt->rect.left() + (vopt->rect.width() - fw) / 2,
                        vopt->rect.top() + 4, fw, row_hint / 2 - 8);

    if (index.row() > 0)
      painter->drawLine(middle, vopt->rect.top(), middle,  frame.top());

    painter->save();

    painter->setBrush(QBrush(dia_col));
    painter->drawRoundedRect(frame, 3.0, 2.0);
    painter->drawText(QPoint(frame.left() + (frame.width() - vopt->fontMetrics.width(text)) / 2,
                             frame.bottom() - (frame.height() - vopt->fontMetrics.ascent()) / 2), text);

    painter->restore();

    if (index.row() == index.model()->rowCount() - 1) {
      painter->drawLine(middle, frame.bottom() + 1, middle, fin_y);

      painter->save();
      painter->setPen(pen_fin);
      painter->drawLine(middle - fin_width, fin_y, middle + fin_width, fin_y);
      painter->restore();
    } else {
      painter->drawLine(middle, frame.bottom() + 1, middle,  vopt->rect.bottom());
    }
  } else {
    if (index.row() == index.model()->rowCount() - 1) {
      painter->drawLine(middle, vopt->rect.top(), middle, fin_y);

      painter->save();
      painter->setPen(pen_fin);
      painter->drawLine(middle - fin_width, fin_y, middle + fin_width, fin_y);
      painter->restore();
    } else {
      painter->drawLine(middle, vopt->rect.top(), middle, vopt->rect.bottom());
    }
  }
}

//! Implements QAbstractItemDelegate::sizeHint.
QSize MSCDelegate::sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const
{
  if (!index.isValid())
    return QSize();

  const QStringList values = index.model()->data(index, MSCModel::enumStatesRole).toStringList();
  
  int width = option.fontMetrics.width(index.model()->headerData(index.column(), Qt::Horizontal).toString());

  foreach(QString str, values) {
    width = qMax(width, option.fontMetrics.width(str));
  }

  return QSize(width + 20, option.fontMetrics.height() * 4);
}

//
// MSCWidget class
//
MSCWidget::MSCWidget(QWidget * parent) : QAbstractItemView(parent)
{
  header_ = new QHeaderView(Qt::Horizontal, this);
//   header_->setMovable(true);
  header_->setSortIndicatorShown(false);
  header_->setStretchLastSection(true);

  connect(header_, SIGNAL(sectionMoved(int, int, int)), SLOT(onColumnMoved(int, int, int)));
  connect(header_, SIGNAL(sectionResized(int, int, int)), SLOT(onColumnResized(int, int, int)));

  MSCDelegate * delegate = new MSCDelegate(this);
  setItemDelegate(delegate);

  setMouseTracking(true);

  setSelectionMode(QAbstractItemView::SingleSelection);
  setSelectionBehavior(QAbstractItemView::SelectRows);

  setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  setAttribute(Qt::WA_MacShowFocusRect);
}

//! Implements QAbstractItemView::indexAt.
QModelIndex MSCWidget::indexAt(const QPoint & point) const
{
  if (!model())
    return QModelIndex();
  
  const int x = point.x() + horizontalOffset();
  const int y = point.y() + verticalOffset();
  const int r_height = rowHeight();

  int col, row;
  row = y / (2 * r_height);

  for (col = -1; col + 1 < header_->count() - 1; ++col) {
    const int idx = header_->visualIndex(col + 1);

    if (x < header_->sectionPosition(idx))
      break;
  }

  QModelIndex index = model()->index(row, col);

  // reverse check
  if (index.isValid() && visualRect(index).contains(point))
    return index;

  return QModelIndex();
}

//! Implements QAbstractItemView::scrollTo.
void MSCWidget::scrollTo(const QModelIndex & index, ScrollHint hint)
{
  if (!index.isValid())
    return;

  // horizontal
  if (horizontalScrollMode() == QAbstractItemView::ScrollPerItem) {
    const int col_index = header_->visualIndex(index.column());

    if (hint == PositionAtCenter) {
      horizontalScrollBar()->setValue(col_index - (horizontalScrollBar()->pageStep() / 2));
    } else if (col_index - horizontalScrollBar()->value() < 0) {
      horizontalScrollBar()->setValue(col_index);
    } else if (col_index > horizontalScrollBar()->pageStep()) {
      horizontalScrollBar()->setValue(col_index - horizontalScrollBar()->pageStep());
    }
  } else {
    const int pos = header_->sectionPosition(index.column());
    const int size = header_->sectionSize(index.column());

    if (hint == PositionAtCenter) {
      horizontalScrollBar()->setValue(pos - ((viewport()->width() - size) / 2));
    } else if (pos - header_->offset() < 0 || size > viewport()->width()) {
      horizontalScrollBar()->setValue(pos);
    } else if (pos - header_->offset() + size > viewport()->width()) {
      horizontalScrollBar()->setValue(pos - viewport()->width() + size);
    }
  }

  // vertical
  if (verticalScrollMode() == QAbstractItemView::ScrollPerItem) {
    if (hint == PositionAtTop || (hint == EnsureVisible && index.row() < verticalScrollBar()->value())) {
      verticalScrollBar()->setValue(index.row());
    } else if (hint == PositionAtBottom || (hint == EnsureVisible &&
                                            index.row() >= verticalScrollBar()->value() + verticalScrollBar()->pageStep())) {
      verticalScrollBar()->setValue(index.row() + 1 - verticalScrollBar()->pageStep());
    } else if (hint == PositionAtCenter) {
      verticalScrollBar()->setValue(index.row() - (verticalScrollBar()->pageStep() / 2));
    }
  } else {
    const int r_height = rowHeight();
    const int item = index.row() * r_height * 2;

    if (hint == PositionAtTop || (hint == EnsureVisible && item < verticalScrollBar()->value())) {
      verticalScrollBar()->setValue(item);
    } else if (hint == PositionAtBottom || (hint == EnsureVisible &&
                                            item >= verticalScrollBar()->value() + verticalScrollBar()->pageStep())) {
      verticalScrollBar()->setValue(item + 2 * r_height - verticalScrollBar()->pageStep());
    } else if (hint == PositionAtCenter) {
      verticalScrollBar()->setValue(item - (verticalScrollBar()->pageStep() / 2));
    }
  }
}

//! Reimplements QAbstractItemView::sizeHintForColumn.
int MSCWidget::sizeHintForColumn(int column) const
{
  if (!model())
    return -1;

  QStyleOptionViewItem option;

  option.initFrom(this);

  const QSize hint = itemDelegate()->sizeHint(option, model()->index(0, column));

  return hint.width();
}

//! Implements QAbstractItemView::visualRect.
QRect MSCWidget::visualRect(const QModelIndex & index) const
{
  if (index.parent() != QModelIndex() || index.row() >= model()->rowCount() ||
      index.column() > model()->columnCount() - 1) {
    return QRect();
  }

  QRect res;

  const int r_height = rowHeight();

  res.setTop(index.row() * 2 * r_height);
  res.setLeft(header_->sectionPosition(index.column()));
  res.setWidth(header_->sectionSize(index.column()));
  res.setHeight(2 * r_height);

  const int h_off = horizontalOffset();
  const int v_off = verticalOffset();
  res.adjust(-h_off, -v_off, -h_off, -v_off);
  return res;
}

/*!
 * Resized the given column according to the hint given by the function
 * sizeHintForColumn.
 */
void MSCWidget::resizeColumnToContents(int column)
{
  if (!model())
    return;

  const int hint = sizeHintForColumn(header_->logicalIndex(column));

  header_->resizeSection(column, hint);
}

//! Reimplements QAbstractItemView::setModel.
void MSCWidget::setModel(QAbstractItemModel * model)
{
  header_->setModel(model);

  QAbstractItemView::setModel(model);

  if (model) {
    hover_ = -1;

    connect(model, SIGNAL(modelReset()), SLOT(resetColumns()));
    connect(model, SIGNAL(modelReset()), SLOT(updateHorizontalScrollBar()));
    connect(model, SIGNAL(modelReset()), SLOT(updateVerticalScrollBar()));
    connect(model, SIGNAL(rowsInserted(QModelIndex, int, int)), SLOT(updateVerticalScrollBar()));
    connect(model, SIGNAL(rowsRemoved(QModelIndex, int, int)), SLOT(updateVerticalScrollBar()));
  } else {
    horizontalScrollBar()->setRange(0, 0);
    verticalScrollBar()->setRange(0, 0);
  }
}

//! Implements QAbstractItemView::horizontalOffset.
int MSCWidget::horizontalOffset() const
{
  if (horizontalScrollMode() == QAbstractItemView::ScrollPerItem) {
    return header_->sectionPosition(
             header_->logicalIndex(horizontalScrollBar()->value()));
  } else {
    return horizontalScrollBar()->value();
  }
}

//! Implements QAbstractItemView::verticalOffset.
int MSCWidget::verticalOffset() const
{
  if (verticalScrollMode() == QAbstractItemView::ScrollPerItem) {
    return verticalScrollBar()->value() * rowHeight() * 2;
  } else {
    return verticalScrollBar()->value();
  }
}

//! Implements QAbstractItemView::isIndexHidden.
bool MSCWidget::isIndexHidden(const QModelIndex & index) const
{
  return false;
}

//! Implements QAbstractItemView::moveCursor.
QModelIndex MSCWidget::moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers)
{
  if (!model())
    return QModelIndex();

  QModelIndex index = currentIndex();

  const int safe_row = index.isValid() ? index.row() : 0;
  const int safe_column = index.isValid() ? index.column() : 0;
  const int page_rows = viewport()->height() / (2 * rowHeight());

  switch (cursorAction) {
    case MovePrevious:
    case MoveUp:
      index = model()->index(qMax(0, index.row() - 1), safe_column);
      break;
    case MoveNext:
    case MoveDown:
      index = model()->index(qMin(model()->rowCount() - 1, index.row() + 1), safe_column);
      break;
    case MoveLeft:
      index = model()->index(safe_row, qMax(0, index.column() - 1));
      break;
    case MoveRight:
      index = model()->index(safe_row, qMin(model()->columnCount() - 1, index.column() + 1));
      break;
    case MoveHome:
      index = model()->index(0, safe_column);
      break;
    case MoveEnd:
      index = model()->index(model()->rowCount() - 1, safe_column);
      break;
    case MovePageUp:
      index = model()->index(qMax(0, index.row() - page_rows), safe_column);
      break;
    case MovePageDown:
      index = model()->index(qMin(model()->rowCount() - 1, index.row() + page_rows), safe_column);
      break;
    default:
      return QModelIndex();
  }

  return index;
}

//! Reimplements QAbstractScrollArea::scrollContentsBy.
void MSCWidget::scrollContentsBy(int dx, int dy)
{
  if (dx) {
    if (horizontalScrollMode() == QAbstractItemView::ScrollPerItem) {
      header_->setOffsetToSectionPosition(horizontalScrollBar()->value());
    } else {
      header_->setOffset(horizontalScrollBar()->value());
    }
  }

  QAbstractItemView::scrollContentsBy(dx, dy);
}

//! Implements QAbstractItemView::setSelection.
void MSCWidget::setSelection(const QRect & rect, QItemSelectionModel::SelectionFlags flags)
{
  if (!selectionModel() || rect.isNull())
    return;

  QItemSelection selection;

  const int h_off = horizontalOffset();
  const int v_off = verticalOffset();
  const int r_height = rowHeight();

  const int from_y = qMax(0, (rect.top() + v_off) / (2 * r_height));
  const int to_y = qMin(model()->rowCount() - 1, (rect.bottom() + v_off) / (2 * r_height));
  
  int from_x, to_x; // visual indices

  for (from_x = -1; from_x < model()->columnCount(); ++from_x) {
    int idx = header_->logicalIndex(from_x + 1);

    if (header_->sectionPosition(idx) > rect.left() + h_off)
      break;
  }

  for (to_x = -1; to_x < model()->columnCount(); ++to_x) {
    int idx = header_->logicalIndex(to_x + 1);

    if (header_->sectionPosition(idx) > rect.right() + h_off)
      break;
  }

  if (header_->sectionsMoved()) {
    for (int i = from_x; i <= to_x; ++i) {
      QItemSelection tmp(model()->index(from_y, header_->logicalIndex(i)),
                         model()->index(to_y, header_->logicalIndex(i)));
      selection.merge(tmp, QItemSelectionModel::Select);
    }
  } else {
    selection.select(model()->index(from_y, from_x), model()->index(to_y, to_x));
  }

  selectionModel()->setCurrentIndex(indexAt(rect.topLeft()), flags);
  selectionModel()->select(selection, flags);
}

//! Implements QAbstractItemView::visualRegionForSelection.
QRegion MSCWidget::visualRegionForSelection(const QItemSelection & selection) const
{
  Q_ASSERT(model());

  QRegion region;

  const int v_off = verticalOffset();
  const int r_height = rowHeight();

  foreach(QItemSelectionRange range, selection) {
    if (header_->sectionsMoved()) {
      for (int i = range.left(); i <= range.right(); ++i) {
        QRect rect = visualRect(model()->index(0, i));

        rect.setTop(range.top() * r_height * 2 - v_off);
        rect.setBottom((range.bottom() + 1) * r_height * 2 - v_off);

        region += rect;
      }
    } else {
      const QRect top_left = visualRect(range.topLeft());
      const QRect bottom_right = visualRect(range.bottomRight());

      region += top_left | bottom_right;
    }
  }

  return region;
}

//! Reimplements QWidget::leaveEvent.
void MSCWidget::leaveEvent(QEvent *)
{
  if (hover_ == -1)
    return;

  const int r_height = rowHeight();
  const int v_off = verticalOffset();

  QRect rect = viewport()->rect();

  rect.setTop(hover_ * r_height * 2 - v_off - 1);
  rect.setBottom((hover_ + 1) * r_height * 2 - v_off + 1);

  hover_ = -1;

  setDirtyRegion(rect);
}

//! Reimplements QAbstractItemView::mouseMoveEvent.
void MSCWidget::mouseMoveEvent(QMouseEvent * event)
{
  QAbstractItemView::mouseMoveEvent(event);

  const int r_height = rowHeight();
  const int v_off = verticalOffset();
  const int row = (event->y() + v_off) / (2 * r_height);

  if (row == hover_)
    return;

  QRegion region;
  QRect rect = viewport()->rect();

  if (hover_ != -1) {
    rect.setTop(hover_ * r_height * 2 - v_off - 1);
    rect.setBottom((hover_ + 1) * r_height * 2 - v_off + 1);
    region += rect;
  }

  hover_ = row;

  rect.setTop(hover_ * r_height * 2 - v_off - 1);
  rect.setBottom((hover_ + 1) * r_height * 2 - v_off + 1);
  region += rect;

  setDirtyRegion(region);
}

//! Reimplements QAbstractScrollArea::paintEvent.
void MSCWidget::paintEvent(QPaintEvent * event)
{
  if (!model())
    return;

  QSet<QModelIndex> indices;
  QSet<int> rows;

  const int r_height = rowHeight();
  const int h_off = horizontalOffset();
  const int v_off = verticalOffset();

  foreach(QRect rect, event->region().rects()) {
    // determine from-to limits
    int from_x, to_x; // visual indices
    int from_y, to_y;

    for (from_x = -1; from_x < model()->columnCount() - 1; ++from_x) {
      int idx = header_->logicalIndex(from_x + 1);

      if (header_->sectionPosition(idx) > rect.left() + h_off)
        break;
    }

    for (to_x = -1; to_x < model()->columnCount() - 1; ++to_x) {
      int idx = header_->logicalIndex(to_x + 1);

      if (header_->sectionPosition(idx) > rect.right() + h_off)
        break;
    }

    from_y = qMax(0, (rect.top() + v_off) / (2 * r_height));
    to_y = qMin(model()->rowCount() - 1 , (rect.bottom() + v_off) / (2 * r_height));

    // add indices to set
    for (int i = from_y; i <= to_y; ++i) {
      rows.insert(i);

      for (int j = from_x; j <= to_x; ++j) {
        QModelIndex index = model()->index(i, header_->logicalIndex(j));

        if (index.isValid() && index.column() < model()->columnCount() - 1)
          indices.insert(index);
      }
    }
  }

  const QPen pen_msg = QPen(QColor(Qt::red));
  const QColor current_col("#a5bfdc");

  QPainter painter(viewport());
  painter.setRenderHint(QPainter::Antialiasing);

  // draw states + connections
  foreach(QModelIndex idx, indices) {
    const QRect rect = visualRect(idx);
    const bool selected = selectionModel()->isSelected(idx);
    const bool current = model()->data(idx, MSCModel::currentStateRole).toBool();

    // draw background
    QStyleOptionViewItemV4 opt = viewOptions();
    opt.state |= QStyle::State_Enabled;

    if (header_->visualIndex(idx.column()) == 0) {
      if (header_->visualIndex(idx.column()) == header_->count() - 2)
        opt.viewItemPosition = QStyleOptionViewItemV4::OnlyOne;
      else
        opt.viewItemPosition = QStyleOptionViewItemV4::Beginning;
    } else if (header_->visualIndex(idx.column()) == header_->count() - 2)
      opt.viewItemPosition = QStyleOptionViewItemV4::End;
    else
      opt.viewItemPosition = QStyleOptionViewItemV4::Middle;

    if (selected)
      opt.state |= QStyle::State_Selected;

    if (hasFocus())
      opt.state |= QStyle::State_HasFocus;

    if (hover_ == idx.row())
      opt.state |= QStyle::State_MouseOver;

    opt.rect = rect;
    opt.index = idx;
    opt.widget = this;

    if (selected)
      opt.backgroundBrush = QBrush(palette().highlight());
    else if (current)
      opt.backgroundBrush = QBrush(current_col);

    itemDelegate()->paint(&painter, opt, idx);
  }

  // draw messages
  foreach(int row, rows) {
    QModelIndex idx = model()->index(row, 0);
    int msgFrom = model()->data(idx, MSCModel::messageFromRole).toInt();
    int msgTo = model()->data(idx, MSCModel::messageToRole).toInt();
    QString message = msgFrom != -1 && msgTo != -1 ?
      model()->data(idx, MSCModel::messageRole).toString() : QString();

    if (msgFrom != -1 && msgTo != -1) {
      int from_x, to_x;
      QRect send_rct, receive_rct;

      const bool left_to_right = header_->visualIndex(msgFrom) < header_->visualIndex(msgTo);

      painter.save();
      painter.setPen(pen_msg);

      idx = model()->index(row, msgFrom);
      send_rct = visualRect(idx);

      idx = model()->index(row, msgTo);
      receive_rct = visualRect(idx);

      // draw connection line
      if (left_to_right) {
        from_x = send_rct.left() + send_rct.width() / 2;
        to_x = receive_rct.left() + receive_rct.width() / 2 - 1;
      } else {
        from_x = send_rct.left() + send_rct.width() / 2 - 1;
        to_x = receive_rct.left() + receive_rct.width() / 2;
      }

      int middle_y = send_rct.bottom() - r_height / 2;

      painter.drawLine(from_x, middle_y, to_x, middle_y);

      // draw arrow
      if (left_to_right) {
        painter.drawLine(to_x, middle_y, to_x - 5, middle_y - 3);
        painter.drawLine(to_x, middle_y, to_x - 5, middle_y + 3);
      } else {
        painter.drawLine(to_x, middle_y, to_x + 5, middle_y - 3);
        painter.drawLine(to_x, middle_y, to_x + 5, middle_y + 3);
      }

      // draw channel name
      const int text_width = QFontMetrics(font()).width(message);
      const QPoint origin(from_x + (to_x - from_x - text_width) / 2, middle_y - 3);

      painter.drawText(origin, message);      
      painter.restore();
    }

    // draw focus rect over entire line
    if (hasFocus() && row == selectionModel()->currentIndex().row()) {
      QRect rect = visualRect(selectionModel()->currentIndex());

      rect.setLeft(-h_off);
      rect.setRight(header_->sectionPosition(header_->count() - 1) - h_off - 1);

      QStyleOptionFocusRect focus;
      focus.initFrom(this);
      focus.rect = rect;

      if (selectionModel()->isSelected(selectionModel()->currentIndex()))
        focus.backgroundColor = palette().color(QPalette::Highlight);

      painter.save();
      painter.setRenderHint(QPainter::Antialiasing, false);
      style()->drawPrimitive(QStyle::PE_FrameFocusRect, &focus, &painter, this);
      painter.restore();
    }
  }
}

//! Reimplements QAbstractItemView::updateGeometries.
void MSCWidget::updateGeometries()
{
  const QSize hint = header_->sizeHint();
  setViewportMargins(0, hint.height(), 0, 0);

  const QRect vg = viewport()->geometry();
  header_->setGeometry(QRect(vg.left(), vg.top() - hint.height(), vg.width(), hint.height()));

  updateHorizontalScrollBar();
  updateVerticalScrollBar();

  QAbstractItemView::updateGeometries();
}

int MSCWidget::rowHeight() const
{
  return QFontMetrics(font()).height() * 2;
}

void MSCWidget::resetColumns()
{
  Q_ASSERT(model());
  
  for (int i = 0; i < model()->columnCount(); ++i)
    resizeColumnToContents(i);

//   disconnect(model(), SIGNAL(modelReset()), this, SLOT(resetColumns()));
}

void MSCWidget::updateHorizontalScrollBar()
{
  if (header_->count() == 0) {
    horizontalScrollBar()->setRange(0, 0);
    return;
  }

  const int width = header_->sectionPosition(header_->count() - 1);

  if (viewport()->width() >= width) {
    horizontalScrollBar()->setRange(0, 0);
    return;
  }

  int shown_cols = 0;

  while (shown_cols < header_->count() - 1 &&
         header_->sectionPosition(header_->logicalIndex(shown_cols)) < viewport()->width()) {
    ++shown_cols;
  }

  if (horizontalScrollMode() == QAbstractItemView::ScrollPerItem) {
    horizontalScrollBar()->setRange(0, header_->count() - shown_cols);
    horizontalScrollBar()->setPageStep(shown_cols);
    horizontalScrollBar()->setSingleStep(1);
  } else {
    horizontalScrollBar()->setRange(0, width - viewport()->width());
    horizontalScrollBar()->setSingleStep(qMax(2, viewport()->width() / (shown_cols + 1)));
    horizontalScrollBar()->setPageStep(viewport()->width());
  }
}

void MSCWidget::updateVerticalScrollBar()
{
  if (!model() || model()->rowCount() == 0) {
    verticalScrollBar()->setRange(0, 0);
    return;
  }

  const int rows = model()->rowCount();
  const int height = rows * rowHeight() * 2;

  if (viewport()->height() >= height) {
    verticalScrollBar()->setRange(0, 0);
    return;
  }

  const int page_rows = qMax(1, viewport()->height() / (2 * rowHeight()));

  if (verticalScrollMode() == QAbstractItemView::ScrollPerItem) {
    verticalScrollBar()->setRange(0, rows - page_rows);
    verticalScrollBar()->setPageStep(page_rows);
    verticalScrollBar()->setSingleStep(1);
  } else {
    verticalScrollBar()->setRange(0, height - viewport()->height());
    verticalScrollBar()->setSingleStep(qMax(2, viewport()->height() / page_rows));
    verticalScrollBar()->setPageStep(viewport()->height());
  }
}

void MSCWidget::onColumnMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex)
{
  // prevent padding from being swapped
  if (newVisualIndex == header_->count() - 1 || oldVisualIndex == header_->count() - 1) {
    SignalLock lock(header_);
    header_->moveSection(newVisualIndex, oldVisualIndex);
    return;
  }

  updateHorizontalScrollBar();

  viewport()->update();
}

void MSCWidget::onColumnResized(int logicalIndex, int oldSize, int newSize)
{
  // ignore padding
  if (logicalIndex == header_->count() - 1)
    return;

  // check minimal size
  const int hint = sizeHintForColumn(logicalIndex);

  if (newSize < hint) {
    SignalLock lock(header_);
    header_->resizeSection(logicalIndex, hint);
  }

  updateHorizontalScrollBar();

  viewport()->update();
}

//
// SequenceDock class
//
MSCDock::MSCDock(MainForm * root) : QDockWidget(tr("Sequence Chart"), root),
  model_(NULL), sim_(root->simulation())
{
  model_ = new MSCModel(sim_, this);
  msc_ = new MSCWidget(this);
  msc_->setModel(model_);
  
  setWidget(msc_);
  
  connect(msc_, SIGNAL(activated(QModelIndex)), SLOT(onItemActivated(QModelIndex)));
}

void MSCDock::onItemActivated(const QModelIndex & index)
{
  sim_->setCurrentIndex(index.row());
}

}
}
