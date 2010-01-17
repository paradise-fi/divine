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

#include "simulator.h"
#include "sevine.h"

class dve_debug_system_t;

/*!
 * This class implements the DVE simulator back-end.
 *
 * Due to the divine::dve_explicit_system_t interface and it's error handling,
 * there must be at most one instance of this class.
 */
class DveSimulator : public Simulator {
  Q_OBJECT

  public:
    //! Returns the single instance of this class.
    static DveSimulator * instance(void) {return instance_;}

  public:
    DveSimulator(QObject * parent);
    ~DveSimulator();
    
    bool loadFile(const QString & path);
    bool isReady(void) const {return system_ != 0;}

    bool loadTrail(const QString & path);
    bool saveTrail(const QString & path) const;
    
    const QStringList files(void) const {return QStringList(path_);}
    
    const SyntaxErrorList errors(void);

    bool isRunning(void) const {return state_ != -1;}
    bool canUndo(void) const {return state_ > 0;}
    bool canRedo(void) const {return state_ >= 0 && state_ < stack_.size() - 1;}

    // states
    int stackDepth(void) const;
    int currentState(void) const;
    bool isAccepting(int state = -1) const;
    bool isErrorneous(int state = -1) const;
    const QPair<int, int> acceptingCycle(void) const;
    
    // capabilities
    int capabilities(void) const {return capVariableMutable;}
    
    // transitions
    int transitionCount(int state = -1) const;
    int usedTransition(int state = -1) const;
    const TransitionList transitionSource(int tid, int state = -1) const;
    const QString transitionName(int tid, int state = -1) const;
    const QStringList transitionText(int tid, int state = -1) const;
    const SyncInfo transitionChannel(int tid, int state = -1) const;
    
    // channels
    int channelCount(void) const;
    const QString channelName(int cid) const;
    const QVariantList channelValue(int cid, int state = -1) const;
    void setChannelValue(int cid, const QVariantList & val);
    
    // processes
    int processCount(void) const;
    const QString processName(int pid) const;
    const QStringList enumProcessStates(int pid) const;
    int processState(int pid, int state = -1) const;
    void setProcessState(int pid, int val);
    
    // variables
    int variableCount(int pid) const;
    const QString variableName(int pid, int vid) const;
    int variableFlags(int pid, int vid, const QVariantList * sub = NULL) const;
    const QVariant variableValue(int pid, int vid, int state = -1) const;
    void setVariableValue(int pid, int vid, const QVariant & val);

    void errorHandler(void);
    
  public slots:
    void start(void);
    void stop(void);
    void undo(void);
    void redo(void);
    void step(int id);
    void backstep(int depth);

  private:
    static DveSimulator * instance_;

  private:
    typedef QList<divine::state_t> TraceStack;

  private:
    divine::error_vector_t err_;
    divine::StateAllocator * generator_;
    dve_debug_system_t * system_;

    // model path
    QString path_;

    // state stack
    TraceStack stack_;
    int state_;
    QPair<int, int> cycle_;

  private:
    void clearSimulation(void);
    void clearStack(void);
    void trimStack(void);
    
    divine::dve_symbol_t * findVariableSymbol(int pid, int vid) const;
    
    void updateAcceptingCycle(void);
    void updateAcceptingCycle(int state);
};

#endif
