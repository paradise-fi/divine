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

#include "modules.h"
#include "moduleManager.h"

namespace divine {
namespace gui {

namespace {
  
  template<typename T>
  struct FactoryLessThan {
    bool operator () (const T * l, const T * r) {
      Q_ASSERT(l && r);
      
      return l->suffix() < r->suffix();
    }
    
    bool operator () (const QString & l, const T * r) {
      return l < r->suffix();
    }
    
    bool operator () (const T * l, const QString & r) {
      return l->suffix() < r;
    }
  };
  
}

ModuleManager::~ModuleManager()
{
}

/*!
 * Registers EditorBuilder instance for given document type.
 * \param factory Factory class instance.
 */
void ModuleManager::registerDocument(AbstractDocumentFactory * factory)
{
  DocumentList::iterator pos = qLowerBound(
    documents_.begin(), documents_.end(), factory->suffix(),
    FactoryLessThan<AbstractDocumentFactory>());
  
  // suffix already taken
  if(pos != documents_.end() && (*pos)->suffix() == factory->suffix())
    return;
  
  documents_.insert(pos, factory);
}

/*!
 * Registers SimulatorFactory instance for given document type.
 * \param factory Factory class instance.
 */
void ModuleManager::registerSimulator(AbstractSimulatorFactory * factory)
{
  SimulatorList::iterator pos = qLowerBound(
    simulators_.begin(), simulators_.end(), factory->suffix(),
    FactoryLessThan<AbstractSimulatorFactory>());
  
  // suffix already taken
  if(pos != simulators_.end() && (*pos)->suffix() == factory->suffix())
    return;
  
  simulators_.insert(pos, factory);
}

const AbstractDocumentFactory * ModuleManager::findDocument(const QString & suffix) const
{
  DocumentList::const_iterator pos = qBinaryFind(
    documents_.begin(), documents_.end(), suffix,
    FactoryLessThan<AbstractDocumentFactory>());
  
  if(pos == documents_.end())
    return NULL;
  
  return *pos;
}

const AbstractSimulatorFactory * ModuleManager::findSimulator(const QString & suffix) const
{
  SimulatorList::const_iterator pos = qBinaryFind(
    simulators_.begin(), simulators_.end(), suffix,
    FactoryLessThan<AbstractSimulatorFactory>());
  
  if(pos == simulators_.end())
    return NULL;
  
  return *pos;
}

}
}
