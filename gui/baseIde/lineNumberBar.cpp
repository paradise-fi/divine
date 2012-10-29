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

#include <QPainter>
#include <QPaintEvent>
#include <QStyleOptionFrameV3>
#include <QAbstractScrollArea>

#include "lineNumberBar.h"

namespace divine {
namespace gui {

LineNumberBar::LineNumberBar(QAbstractScrollArea * parent, LineNumberBarHost * host)
  : QWidget(parent), host_(host)
{
  setObjectName("lineNumberBar");
  setAutoFillBackground(true);
}

LineNumberBar::~LineNumberBar()
{
}

void LineNumberBar::lineCountChanged()
{
  updateGeometry();
  update();
}

void LineNumberBar::updateGeometry()
{
  if(!isVisible())
    return;
  
  // get new width
  int nums = QString::number(host_->lineCount()).length();

  if (nums < 2)
    nums = 2;

  int newWidth = fontMetrics().width(QString(nums + 1, '0'))/* + 6*/;

  QStyleOptionFrameV3 option;
  option.initFrom(parentWidget());

  QRect client = style()->subElementRect(
                   QStyle::SE_FrameContents, &option, parentWidget());

  QAbstractScrollArea * parent = qobject_cast<QAbstractScrollArea*>(parentWidget());
  Q_ASSERT(parent);
  
  QRect view = parent->viewport()->rect();
  QRect visual = QStyle::visualRect(layoutDirection(), view, QRect(
    client.left(), client.top(), newWidth, view.height() - view.top()));
  
  if(newWidth != width())
    emit widthChanged(newWidth);
  
  if(visual != geometry())
    setGeometry(visual);
}

void LineNumberBar::changeEvent(QEvent * event)
{
  QWidget::changeEvent(event);
  
  if (event->type() == QEvent::ApplicationFontChange ||
      event->type() == QEvent::FontChange) {

    updateGeometry();
  }
}

void LineNumberBar::hideEvent(QHideEvent * event)
{
  // pretend we have disappeared
  emit widthChanged(0);
}

void LineNumberBar::showEvent(QShowEvent * event)
{
  lineCountChanged();
  emit widthChanged(width());
}

void LineNumberBar::paintEvent(QPaintEvent * event)
{
  if(!isVisible())
    return;
  
  QPainter painter(this);
  
  const int barWidth = width();
  const int fontHeight = painter.fontMetrics().height();
  
  QList<int> lines = host_->visibleLines(event->rect());
  int lineNumber = host_->firstVisibleLine(event->rect());

  foreach(int top, lines) {
    painter.drawText(0, top, barWidth - 4, fontHeight,
                     Qt::AlignRight, QString::number(lineNumber + 1));
    lineNumber += 1;
  }
}

}
}
