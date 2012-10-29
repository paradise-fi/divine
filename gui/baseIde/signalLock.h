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

#ifndef SIGNAL_LOCK_H_
#define SIGNAL_LOCK_H_

#include <QObject>

namespace divine {
namespace gui {

class SignalLock {
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
}

#endif

