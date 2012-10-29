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

#ifndef PLUGIN_MANAGER_H_
#define PLUGIN_MANAGER_H_

#include <QObject>
#include <QString>
#include <QIcon>
#include <QList>
#include <QPair>

class QIcon;

namespace divine {
namespace gui {

class AbstractDocumentFactory;
class AbstractSimulatorFactory;
  
class PluginManager : public QObject {
  Q_OBJECT

public:
  typedef QList<AbstractDocumentFactory*> DocumentList;
  typedef QList<AbstractSimulatorFactory*> SimulatorList;
  
public:
  PluginManager(QObject * parent = NULL) : QObject(parent) {}
  ~PluginManager();

  void registerDocument(AbstractDocumentFactory * factory);
  void registerSimulator(AbstractSimulatorFactory * loader);

  const DocumentList & documents() const {return documents_;}
  const SimulatorList & simulators() const {return simulators_;}
  
  const AbstractDocumentFactory * findDocument(const QString & suffix) const;
  const AbstractSimulatorFactory * findSimulator(const QString & suffix) const;
    
private:
  DocumentList documents_;
  SimulatorList simulators_;
};

}
}

#endif
