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

#ifndef ABSTRACT_SIMULATOR_H_
#define ABSTRACT_SIMULATOR_H_

#include "baseIdeExport.h"

#include <QObject>
#include <QList>
#include <QVariant>
#include <QString>
#include <QIcon>

namespace divine {
namespace gui {

class AbstractDocument;

//
// Symbol table
//
struct BASE_IDE_EXPORT SymbolType {
  virtual ~SymbolType() {}
  
  QString id;
  QString name;
};

struct BASE_IDE_EXPORT PODType : public SymbolType {
  enum BaseType {
    Integer,
    Char,
    Float,
    Double,
    String
  } podType;
  
  qint64 min;
  qint64 max;
};

struct BASE_IDE_EXPORT ArrayType : public SymbolType {
  QString subtype;
  int size;
};

struct BASE_IDE_EXPORT ListType : public SymbolType {
};

struct BASE_IDE_EXPORT OptionType : public SymbolType {
  QStringList options;
};

//
// Other structures
//
struct BASE_IDE_EXPORT Symbol {
  static const quint32 NoFlag = 0x00;
  static const quint32 System = 0x01;
  static const quint32 ReadOnly = 0x02;
  static const quint32 Pinned = 0x04;   // enforces symbol's visibility in certain widgets
  
  virtual ~Symbol() {}
  
  QString id;
  QString type;
  QString name;
  QIcon icon;
  
  QString parent;
  QList<QString> children;
  
  quint32 flags;
  
  bool hasFlag(quint32 flag) const { return (flags & flag) != 0; }
};

struct BASE_IDE_EXPORT Communication {
  virtual ~Communication() {}
  
  QString from;
  QString to;
  QString medium;
};

struct BASE_IDE_EXPORT Transition {
  virtual ~Transition() {}
  
  QString description;
};

//
// Simulator interface
//
// NOTE: pointers to structures might not be valid forever
//
class BASE_IDE_EXPORT AbstractSimulator : public QObject
{
    Q_OBJECT
  
  public:
    AbstractSimulator(QObject * parent = 0) : QObject(parent) {}
    virtual ~AbstractSimulator() {}
    
    // loading
    // TODO: convert to exceptions?
    virtual bool loadDocument(AbstractDocument * doc) = 0;
    
    // stack
    virtual int stateCount() const = 0;
    
    virtual bool isAccepting(int state) = 0;
    virtual bool isValid(int state) = 0;
    
    virtual QPair<int, int> findAcceptingCycle() = 0;
    
    // transitions
    virtual int transitionCount(int state) = 0;
    virtual const Transition * transition(int index, int state) = 0;
    virtual QList<const Communication*> communication(int index, int state) = 0;
    virtual QList<const Transition*> transitions(int state) = 0;
    
    virtual int usedTransition(int state) = 0;
    
    // symbol table
    virtual const SymbolType * typeInfo(const QString & id) const = 0;
    
    // state access
    virtual const Symbol * symbol(const QString & id, int state) = 0;
    virtual QStringList topLevelSymbols(int state) = 0;
    virtual QVariant symbolValue(const QString & id, int state) = 0;
    virtual bool setSymbolValue(const QString & id, const QVariant & value, int state) = 0;
    
  public slots:
    // simulation controls
    virtual void start() = 0;
    virtual void stop() = 0;
    
    // removes all states with higher index
    virtual void trimStack(int topIndex) = 0;
    
    /*!
     * Makes a step in the simulation.
     * \param id ID of the transition to be used.
     */
    virtual void step(int id) = 0;

  signals:
    void message(const QString & msg);
    void stateChanged(int index);
};

}
}

#endif
