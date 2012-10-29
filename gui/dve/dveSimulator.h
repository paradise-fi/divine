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

#ifndef DVE_SIMULATOR_H_
#define DVE_SIMULATOR_H_

#include <divine/toolkit/blob.h>

#include <QHash>
#include <QRect>

#include "abstractSimulator.h"

inline uint qHash(const QRect & rect)
{
  return rect.x() + 37*rect.y() + 43*rect.width() + 47*rect.height();
}

namespace divine {

namespace dve {
  struct System;
  typedef int SymId;
}

namespace gui {

struct DveSymbol : public Symbol {
  dve::SymId dveId;
};

struct DveTransition : public Transition {
  Communication sync;
  QList<QRect> source;
};


/*!
 * This class implements the DVE simulator back-end.
 */
class DveSimulator : public AbstractSimulator {
  Q_OBJECT

  public:
    DveSimulator(QObject * parent);
    ~DveSimulator();
    
    bool loadDocument(AbstractDocument * doc);
    bool loadSystem(const QString & path);
    void closeSystem();
    
    // stack
    int stateCount() const;
    int currentState() const;
    
    bool isAccepting(int state);
    bool isValid(int state);
    
    QPair<int, int> findAcceptingCycle();
    
    // transitions
    int transitionCount(int state);
    const Transition * transition(int index, int state);
    QList<const Communication*> communication(int index, int state);
    QList<const Transition*> transitions(int state);
    
    int usedTransition(int state);
    
    // symbol table
    const SymbolType * typeInfo(const QString & id) const;
    
    // state access
    const Symbol * symbol(const QString & id, int state);
    QStringList topLevelSymbols(int state);
    QVariant symbolValue(const QString & id, int state);
    bool setSymbolValue(const QString & id, const QVariant & value, int state);
    
  public slots:
    // simulation controls
    void start();
    void stop();
    void trimStack(int topIndex);
    void step(int id);

  private:
    dve::System * system_;
    
    QList<Blob> stack_;
    
    QHash<QString, SymbolType*> types_;
    QHash<QString, DveSymbol*> symbols_;
    QHash<dve::SymId, QString> reverse_;    // for reverse lookup of processes
    QList<QString> topLevel_;
    
    QList<DveTransition> transitions_;
    int bufferedState_;

    QHash<QRect, QString> sources_;         // lazily updated map with blocks of source code
    QString path_;
    
  private:
    Blob newBlob();
    int stateSize();
    
    int findAcceptingCycle(int to);
    
    void updateTransitions(int state);
    
    void createSymbolTable();
    void clearSymbolTable();
    
    QString getSourceBlock(const QRect & rect);
};

}
}

#endif
