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

#include <QFont>
#include <QPainter>
#include <QPaintEvent>
#include <QStyleOptionFrameV3>
#include <QAbstractScrollArea>

#include "lineNumberBar.h"
#include "cycleBar.h"

namespace divine {
namespace gui {

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


CycleBar::CycleBar(QAbstractScrollArea * parent, LineNumberBarHost * host)
  : QWidget(parent), host_(host), cycle_(-1, -1), margin_(0)
{
  setObjectName("cycleBar");
  setAutoFillBackground(true);
}

void CycleBar::setCycle(const QPair<int, int> & cycle)
{
  if (cycle != cycle_) {
    cycle_ = cycle;
    update();
  }
}

void CycleBar::setMargin(int margin)
{
  if(margin != margin_) {
    margin_ = margin;
    updateGeometry();
  }
}

void CycleBar::updateGeometry()
{
  QStyleOptionFrameV3 option;
  option.initFrom(parentWidget());

  QRect client = style()->subElementRect(
                   QStyle::SE_FrameContents, &option, parentWidget());

  QAbstractScrollArea * parent = qobject_cast<QAbstractScrollArea*>(parentWidget());
  Q_ASSERT(parent);

  QRect view = parent->viewport()->rect();
  QRect visual = QStyle::visualRect(layoutDirection(), view, QRect(
    client.left() + margin_, client.top(), cycleBarWidth, view.height() - view.top()));

  if(visual != geometry())
    setGeometry(visual);
}

void CycleBar::changeEvent(QEvent * event)
{
  QWidget::changeEvent(event);

  if (event->type() == QEvent::ApplicationFontChange ||
      event->type() == QEvent::FontChange) {

    updateGeometry();
  }
}

void CycleBar::paintEvent(QPaintEvent * event)
{
  if(!isVisible())
    return;

  // don't paint anything if there are no cycles
  if(cycle_.first < 0 || cycle_.second < 0)
    return;

  QPainter painter(this);

  QPen thin, thick;
  thick.setWidth(2);

  QBrush brush;
  brush.setColor(Qt::black);
  brush.setStyle(Qt::SolidPattern);

  painter.setPen(thin);
  painter.setBrush(brush);

  QList<int> lines = host_->visibleLines(event->rect());
  int firstLine = host_->firstVisibleLine(event->rect());

  const int fontHeight = painter.fontMetrics().height();

  for(int i=0; i < lines.count(); ++i) {
    int line = i + firstLine;

    if(line < cycle_.first || line > cycle_.second)
      continue;

    if(line == cycle_.first) {
      painter.drawLine(cycleLeftOffset, lines[i] + fontHeight,
                       cycleLeftOffset, lines[i] + fontHeight / 2);
      painter.drawLine(cycleLeftOffset, lines[i] + fontHeight / 2,
                       cycleBarWidth - 2, lines[i] + fontHeight / 2);

      // arrow
      QPolygon poly;
      poly.putPoints(0, 3,
                     cycleBarWidth - 4, lines[i] + fontHeight / 2 - 3,
                     cycleBarWidth - 1, lines[i] + fontHeight / 2,
                     cycleBarWidth - 4, lines[i] + fontHeight / 2 + 3);
      painter.drawPolygon(poly);
    } else if(line == cycle_.second) {
      painter.setPen(thick);
      painter.drawLine(cycleBarWidth - 2, lines[i] + 3,
                       cycleBarWidth - 2, lines[i] + fontHeight - 2);
      painter.setPen(thin);
      painter.drawLine(cycleBarWidth - 2, lines[i] + fontHeight / 2,
                       cycleLeftOffset, lines[i] + fontHeight / 2);
      painter.drawLine(cycleLeftOffset, lines[i] + fontHeight / 2,
                       cycleLeftOffset, lines[i]);
    } else {
      painter.drawLine(cycleLeftOffset, lines[i],
                       cycleLeftOffset, lines[i] + fontHeight);
    }
  }
}

}
}
