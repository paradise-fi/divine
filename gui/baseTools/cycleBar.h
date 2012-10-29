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

#ifndef CYCLE_BAR_H_
#define CYCLE_BAR_H_

#include <QWidget>
#include <QPair>

class QAbstractScrollArea;

namespace divine {
namespace gui {

class LineNumberBarHost;
  
class CycleBar : public QWidget
{
  Q_OBJECT
  
  public:
    CycleBar(QAbstractScrollArea * parent, LineNumberBarHost * host);

    const QPair<int, int> & cycle() const {return cycle_;}
    
  public slots:
    void setCycle(const QPair<int, int> & cycle);
    void setMargin(int margin);
    void updateGeometry();
    
  protected:
    void changeEvent(QEvent * event);
    void paintEvent(QPaintEvent * event);

  private:
    LineNumberBarHost * host_;
    QPair<int, int> cycle_;
    int margin_;
};

}
}

#endif
