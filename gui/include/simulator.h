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

#ifndef SIMULATOR_H_
#define SIMULATOR_H_

#include "base_shared_export.h"

#include <QObject>
#include <QPair>
#include <QList>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QVariant>

class BASE_SHARED_EXPORT SyncInfo
{
  public:
    SyncInfo() : sender_(-1), receiver_(-1) {}

    SyncInfo(int sender, int receiver, const QString & channel)
      : sender_(sender), receiver_(receiver), channel_(channel)
    {
    }
    
    bool isValid(void) const
    {
      return sender_ >= 0 && receiver_ >= 0 && sender_ != receiver_;
    }
    
    int sender(void) const {return sender_;}
    int receiver(void) const {return receiver_;}
    const QString & channel(void) const {return channel_;}

  private:
    int sender_;
    int receiver_;
    QString channel_;
};

Q_DECLARE_METATYPE(SyncInfo);

struct SyntaxError
{
  QString file;
  QRect block;
  QString message;
};

typedef QList<SyntaxError> SyntaxErrorList;

class BASE_SHARED_EXPORT Simulator : public QObject
{
    Q_OBJECT

  public:
    // var type flags
    static const int varConst = 0x100;
    static const int varResizeable = 0x200;
    
    // capabilities
    static const int capChannelMutable = 0x01;
    static const int capProcessMutable = 0x02;
    static const int capVariableMutable = 0x04;
    
    // reserved IDs
    static const int invalidId = -2;
    static const int globalId = -1;

    // transition types
    typedef QPair<QString, QRect> TransitionPair;   // [file, pos]
    typedef QList<TransitionPair> TransitionList;
    
  public:
    Simulator(QObject * parent = NULL) : QObject(parent) {}
    virtual ~Simulator() {}

    virtual bool openFile(const QString & fileName) = 0;
    virtual bool isOpen(void) const = 0;
    
    virtual bool loadTrail(const QString & fileName) = 0;
    virtual bool saveTrail(const QString & fileName) const = 0;

    // returns list of absolute filenames (paths)
    virtual const QStringList files(void) const = 0;

    // retrieve errors
    virtual const SyntaxErrorList errors(void) = 0;
    
    virtual bool isRunning(void) const = 0;
    virtual bool canUndo(void) const = 0;
    virtual bool canRedo(void) const = 0;

    // capabilities
    virtual int capabilities(void) const = 0;
    
    // states
    virtual int traceDepth(void) const = 0;
    virtual int currentState(void) const = 0;
    virtual bool isAccepting(int state = -1) const = 0;
    virtual bool isErrorneous(int state = -1) const = 0;
    virtual const QPair<int, int> acceptingCycle(void) const = 0;
    
    // transitions
    virtual int transitionCount(int state = -1) const = 0;
    virtual int usedTransition(int state = -1) const = 0;
    virtual const TransitionList transitionSource(int tid, int state = -1) const = 0;
    virtual const QString transitionName(int tid, int state = -1) const = 0;
    virtual const QStringList transitionText(int tid, int state = -1) const = 0;
    virtual const SyncInfo transitionChannel(int tid, int state = -1) const  = 0;
    
    // channels
    virtual int channelCount(void) const = 0;
    virtual const QString channelName(int cid) const = 0;
    virtual const QVariantList channelValue(int cid, int state = -1) const = 0;
    virtual void setChannelValue(int cid, const QVariantList & val) = 0;
    
    // processes
    virtual int processCount(void) const = 0;
    virtual const QString processName(int pid) const = 0;
    virtual const QStringList enumProcessStates(int pid) const = 0;
    virtual int processState(int pid, int state = -1) const = 0;
    virtual void setProcessState(int pid, int val) = 0;
    
    // variables
    virtual int variableCount(int pid) const = 0;
    virtual const QString variableName(int pid, int vid) const = 0;
    virtual int variableFlags(int pid, int vid, const QVariantList * sub = NULL) const = 0;
    virtual const QVariant variableValue(int pid, int vid, int state = -1) const = 0;
    virtual void setVariableValue(int pid, int vid, const QVariant & val) = 0;
    
  public slots:
    virtual void start(void) = 0;
    virtual void restart(void) = 0;
    virtual void stop(void) = 0;
    virtual void undo(void) = 0;
    virtual void redo(void) = 0;
    virtual void step(int id) = 0;
    virtual void backtrace(int depth) = 0;

  signals:
    // error propagation
    void message(const QString & msg);

    // stopped or crashed
    void started(void);
    void stopped(void);
    
    // general state update
    void stateReset(void);
    void stateChanged(void);
};

class BASE_SHARED_EXPORT SimulatorProxy : public QObject
{
  Q_OBJECT
  
  public:
    SimulatorProxy(Simulator * sim, QObject * parent = NULL);
    ~SimulatorProxy();
    
    bool lock(QObject * owner);
    bool release(QObject * owner);
    
    bool isLocked(void) const {return lock_ != NULL;}
    
    // const functions can be directly accessed
    const Simulator * simulator(void) const {return sim_;}
    
    // non-const functions are delegated
    void setChannelValue(int cid, const QVariantList & val, QObject * owner = NULL);
    void setProcessState(int pid, int val, QObject * owner = NULL);
    void setVariableValue(int pid, int vid, const QVariant & val, QObject * owner = NULL);
    
  public slots:
    void start(void);
    void restart(void);
    void stop(void);
    void undo(QObject * owner = NULL);
    void redo(QObject * owner = NULL);
    void step(int id, QObject * owner = NULL);
    void backtrace(int depth, QObject * owner = NULL);
    
  signals:
    void locked(bool lock);
    
  private:
    Simulator * sim_;
    QObject * lock_;
};

#endif
