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

#include "trace.h"
#include "simulator.h"
#include "settings.h"
#include "print.h"

// defaults
#include "base_tools.h"

namespace
{

  const int cycleBarWidth = 15;

  // cycle painting constants
  const int cycleBaseSize = 8;
  const int cycleLeftOffset = 7;

}

/*!
 * This class implements the side bar with accepting cycle and line numbers.
 */
class CycleBar : public QWidget
{
  public:
    CycleBar(TraceWidget * parent);

  protected:
    void paintEvent(QPaintEvent * event);

  private:
    TraceWidget * stack_;
};

CycleBar::CycleBar(TraceWidget * parent) : QWidget(parent), stack_(parent)
{
}

//! Forwards the paint event to the TraceWidget class.
void CycleBar::paintEvent(QPaintEvent * event)
{
  stack_->sideBarPaintEvent(event);
}

TraceWidget::TraceWidget(QWidget * parent)
    : QListWidget(parent), cycle_(-1, -1)
{
  setViewportMargins(cycleBarWidth, 0, 0, 0);
  cbar_ = new CycleBar(this);
}

//! Set the displayed accepting cycle.
void TraceWidget::setAcceptingCycle(const QPair<int, int> & cycle)
{
  if (cycle != cycle_) {
    cycle_ = cycle;
    update();
  }
}

//! Reimplements QListView::resizeEvent.
void TraceWidget::resizeEvent(QResizeEvent * event)
{
  QListWidget::resizeEvent(event);

  QStyleOptionFrameV3 option;
  option.initFrom(this);

  const QRect client = style()->subElementRect(
                        QStyle::SE_FrameContents, &option, this);

  const QRect view = viewport()->rect();
  const QRect visual = QStyle::visualRect(layoutDirection(), view, QRect(
    client.left(), client.top(), cycleBarWidth, view.height() - view.top()));

  cbar_->setGeometry(visual);
}

//! Reimplements QAbstractScrollArea::scrollContentsBy.
void TraceWidget::scrollContentsBy(int dx, int dy)
{
  QListWidget::scrollContentsBy(dx, dy);

  cbar_->update();
}

void TraceWidget::sideBarPaintEvent(QPaintEvent * event)
{
  // don't paint anything if there are no cycles
  if (cycle_.first < 0 || cycle_.second < 0)
    return;

  QPainter painter(cbar_);

  QRect rct, first_rct, last_rct;
  int first_id = -1, last_id = -1;

  for (int i = 0; i < count(); ++i) {
    rct = visualItemRect(item(i));

    if (rct.top() > event->rect().bottom())
      break;

    if (rct.bottom() <= event->rect().top())
      continue;

    if (i >= cycle_.second && i <= cycle_.first) {
      if (first_id == -1) {
        first_id = i;
        first_rct = rct;
      }

      last_id = i;
      last_rct = rct;
    }
  }

  // draw nothing
  if (first_id == -1)
    return;

  QList<QPolygon> lines;
  QPolygon line;

  // start of the cycle
  if (last_id == cycle_.first) {
    const int base_off = last_rct.top() + (last_rct.height() - cycleBaseSize) / 2;
    const int line_off = last_rct.top() + last_rct.height() / 2;

    line.push_back(QPoint(cycleBarWidth - 2, base_off));
    line.push_back(QPoint(cycleBarWidth - 2, base_off + cycleBaseSize));
    lines.append(line);

    line.clear();
    line.push_back(QPoint(cycleBarWidth - 2, line_off));
    line.push_back(QPoint(cycleLeftOffset, line_off));
  } else {
    line.push_back(QPoint(cycleLeftOffset, last_rct.bottom()));
  }

  const int line_off = first_rct.top() + first_rct.height() / 2;

  // end

  if (first_id == cycle_.second) {
    line.push_back(QPoint(cycleLeftOffset, line_off));
    line.push_back(QPoint(cycleBarWidth - 2, line_off));
    lines.append(line);

    // arrow
    line.clear();
    line.push_back(QPoint(cycleBarWidth - 4, line_off - 2));
    line.push_back(QPoint(cycleBarWidth - 2, line_off));
    line.push_back(QPoint(cycleBarWidth - 4, line_off + 2));
    lines.append(line);
  } else {
    line.push_back(QPoint(cycleLeftOffset, first_rct.top()));
    lines.append(line);
  }

  QPen pen;

  painter.setPen(pen);
  foreach(QPolygon itr, lines) {
    painter.drawPolyline(itr);
  }
}

TraceDock::TraceDock(QWidget * parent)
    : QDockWidget(tr("Stack trace"), parent), sim_(NULL)
{
  trace_ = new TraceWidget(this);
  setWidget(trace_);

  readSettings();

  connect(trace_, SIGNAL(itemActivated(QListWidgetItem*)),
          SLOT(onItemActivated(QListWidgetItem*)));
}

//! Reload settings.
void TraceDock::readSettings(void)
{
  QSettings & s = sSettings();

  s.beginGroup("Stack");

  variable_ = s.value("variable", defTraceVars).toBool();
  variableName_ = s.value("variableName", defTraceVarNames).toBool();
  process_ = s.value("process", defTraceProcs).toBool();
  processName_ = s.value("processName", defTraceProcNames).toBool();
  channel_ = s.value("channel", defTraceBufs).toBool();
  channelName_ = s.value("channelName", defTraceBufNames).toBool();

  deadlock_ = QColor(s.value("deadlock", defTraceDeadlock).value<QColor>());
  error_ = QColor(s.value("error", defTraceError).value<QColor>());

  s.endGroup();

  if (sim_)
    onSimulatorStarted();
}

//! Update current simulator.
void TraceDock::setSimulator(SimulatorProxy * sim)
{
  if (sim_ == sim)
    return;

  sim_ = sim;

  if (sim_) {
    connect(sim_, SIGNAL(started()), SLOT(onSimulatorStarted()));
    connect(sim_, SIGNAL(stateChanged()), SLOT(onSimulatorStateChanged()));
  } else {
    trace_->clear();
    trace_->setAcceptingCycle(qMakePair(-1, -1));
  }
}

void TraceDock::updateTraceItem(int state)
{
  Q_ASSERT(sim_);

  QString text;

  const Simulator * simulator = sim_->simulator();

  if (channel_ || variable_) {
    text.append("[");

    // channels

    if (channel_) {
      for (int i = 0; i < simulator->channelCount(); ++i) {
        if (channelName_) {
          text.append(simulator->channelName(i));
          text.append(":");
        }

        text.append(prettyPrint(simulator->channelValue(i, state)));

        if (i < simulator->channelCount() - 1 ||
            simulator->variableCount(Simulator::globalId) > 0) {
          text.append(", ");
        }
      }
    }

    // globals
    if (variable_) {
      for (int i = 0; i < simulator->variableCount(Simulator::globalId); ++i) {
        if (variableName_) {
          text.append(simulator->variableName(Simulator::globalId, i));
          text.append(": ");
        }

        text.append(prettyPrint(simulator->variableValue(Simulator::globalId, i, state)));

        if (i < simulator->variableCount(Simulator::globalId) - 1)
          text.append(", ");
      }
    }

    text.append("]");

    if (simulator->processCount() > 0)
      text.append("; ");
  }

  // processes
  if (process_) {
    for (int i = 0; i < simulator->processCount(); ++i) {
      if (processName_) {
        text.append(simulator->processName(i));
        text.append(": ");
      }

      // NOTE: Divine reports invalid state value for errorneous processes
      QStringList state_lst = simulator->enumProcessStates(i);
      const int sid = simulator->processState(i, state);
      if (sid >= 0 && sid < state_lst.size())
        text.append(state_lst.at(sid));
      else
        text.append(tr("-Err-"));

      if (variable_) {
        text.append("[");

        for (int j = 0; j < simulator->variableCount(i); ++j) {
          if (variableName_) {
            text.append(simulator->variableName(i, j));
            text.append(":");
          }

          text.append(prettyPrint(simulator->variableValue(i, j, state)));

          if (j < simulator->variableCount(i) - 1)
            text.append(", ");
        }

        text.append("]");
      }

      if (i < simulator->processCount() - 1)
        text.append("; ");
    }
  }

  QListWidgetItem * item = trace_->item(state);

  item->setText(text);

  // update font
  QFont fnt = item->font();
  fnt.setWeight(state == simulator->currentState() ? QFont::Bold : QFont::Normal);
  fnt.setItalic(simulator->isAccepting(state));
  item->setFont(fnt);

  // and background colour
  QPalette def;

  if (simulator->isErrorneous(state)) {
    item->setForeground(error_);
  } else if (simulator->transitionCount(state) == 0) {
    item->setForeground(deadlock_);
  } else {
    item->setForeground(def.color(QPalette::Text));
  }
}

void TraceDock::onSimulatorStarted(void)
{
  Q_ASSERT(sim_);

  const Simulator * simulator = sim_->simulator();

  // equalize item count - remove items
  while (trace_->count() > simulator->stackDepth()) {
    QListWidgetItem * item = trace_->takeItem(trace_->count() - 1);
    delete item;
  }

  // add items
  while (trace_->count() < simulator->stackDepth()) {
    trace_->addItem("");
  }

  state_ = simulator->currentState();

  trace_->setAcceptingCycle(simulator->acceptingCycle());

  for (int i = 0; i < trace_->count(); ++i) {
    updateTraceItem(i);
  }

  trace_->scrollToItem(trace_->item(state_));
}

void TraceDock::onSimulatorStateChanged(void)
{
  Q_ASSERT(sim_);

  const Simulator * simulator = sim_->simulator();

  // equalize item count - remove items
  while (trace_->count() > simulator->stackDepth()) {
    QListWidgetItem * item = trace_->takeItem(trace_->count() - 1);
    delete item;
  }

  // add items
  while (trace_->count() < simulator->stackDepth()) {
    trace_->addItem("");
    updateTraceItem(trace_->count() - 1);
  }

  // unmark previous active state
  Q_ASSERT(state_ < simulator->stackDepth());

  QListWidgetItem * item = trace_->item(state_);

  QFont fnt = item->font();

  fnt.setWeight(QFont::Normal);

  item->setFont(fnt);

  state_ = simulator->currentState();
  trace_->setAcceptingCycle(simulator->acceptingCycle());

  // update & mark current row
  updateTraceItem(state_);

  // scroll to active row
  trace_->scrollToItem(trace_->item(state_));
}

void TraceDock::onItemActivated(QListWidgetItem * item)
{
  if (sim_)
    sim_->backstep(trace_->row(item));
}
