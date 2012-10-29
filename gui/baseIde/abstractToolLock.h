/***************************************************************************
 *   Copyright (C) 2012 by Martin Moracek                                  *
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

#ifndef ABSTRACT_TOOL_LOCK_H_
#define ABSTRACT_TOOL_LOCK_H_

#include "baseIdeExport.h"

#include <QObject>

namespace divine {
namespace gui {

class AbstractDocument;

class BASE_IDE_EXPORT AbstractToolLock
{
  public:
    AbstractToolLock() {}
    virtual ~AbstractToolLock() {}

    virtual bool isLocked(AbstractDocument * document) const = 0;

    virtual void lock(AbstractDocument * document) = 0;

    virtual bool maybeRelease() = 0;
    virtual void forceRelease() = 0;
};

}
}

#endif
