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

#ifndef LINE_NUMBER_BAR_H_
#define LINE_NUMBER_BAR_H_

#include <QWidget>

class QRect;
class QAbstractScrollArea;

namespace divine {
namespace gui {

class LineNumberBarHost {
  public:
    virtual int lineCount() = 0;
    
    virtual int firstVisibleLine(const QRect & rect) = 0;
    virtual QList<int> visibleLines(const QRect & rect) = 0;
};

class LineNumberBar : public QWidget
{
    Q_OBJECT

  public:
    LineNumberBar(QAbstractScrollArea * parent, LineNumberBarHost * host);
    ~LineNumberBar();

  public slots:
    void lineCountChanged();
    void updateGeometry();
    
  signals:
    void widthChanged(int width);
    
  protected:
    void changeEvent(QEvent * event);
    void hideEvent(QHideEvent * event);
    void showEvent(QShowEvent * event);
    void paintEvent(QPaintEvent * event);
    
  private:
    LineNumberBarHost * host_;
};

}
}

#endif
