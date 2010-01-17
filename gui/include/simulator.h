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

/*!
 * The SyncInfo class stores information about synchronisation between two
 * processes.
 */
class BASE_SHARED_EXPORT SyncInfo
{
  public:
    SyncInfo() : sender_(-1), receiver_(-1) {}

    //! Initializes and fills the SyncInfo structure.
    SyncInfo(int sender, int receiver, const QString & channel)
      : sender_(sender), receiver_(receiver), channel_(channel)
    {
    }
    
    /*! Checks if this instance was initialized correctly (this is an
     *  equivalent of a testing for a NULL value).
     */
    bool isValid(void) const
    {
      return sender_ >= 0 && receiver_ >= 0 && sender_ != receiver_;
    }
    
    //! Gets sender ID
    int sender(void) const {return sender_;}
    
    //! Gets receiver ID
    int receiver(void) const {return receiver_;}
    
    //! Gets the name of the communication channel
    const QString & channel(void) const {return channel_;}

  private:
    int sender_;
    int receiver_;
    QString channel_;
};

Q_DECLARE_METATYPE(SyncInfo);

/*!
 * The SyntaxError structure contains information about a syntax error.
 */
struct SyntaxError
{
  //! File where the error occured.
  QString file;
  //! Position in the code.
  QRect block;
  //! Error message.
  QString message;
};

typedef QList<SyntaxError> SyntaxErrorList;

/*!
 * The Simulator class is an interface for all simulator implementations.
 *
 * The interface is expects a fixed number of processes throughout the
 * simulation. Processes are referenced through global IDs, variables and
 * and channels through process-relative IDs. Special process ID indicates
 * global space.
 *
 * Implicit values of state arguments (-1) mean that the current state should
 * be used.
 */
class BASE_SHARED_EXPORT Simulator : public QObject
{
    Q_OBJECT

  public:
    // var type flags
    static const int varConst = 0x100;   //!< Flag indicating constant variable.
//     static const int varResizeable = 0x200;
    
    // capabilities
    //! The simulator is capable of changing channel contents.
    static const int capChannelMutable = 0x01;
    //! The simulator is capable of changing process states.
    static const int capProcessMutable = 0x02;
    //! The simulator is capable of changing variable values.
    static const int capVariableMutable = 0x04;
    
    // reserved IDs
    static const int invalidId = -2;    //!< ID is invalid.
    //! ID references global space instead of a process.
    static const int globalId = -1;     

    // transition types
    //! Stores transition information [file, position].
    typedef QPair<QString, QRect> TransitionPair;
    //! List of transitions.
    typedef QList<TransitionPair> TransitionList;
    
  public:
    Simulator(QObject * parent = NULL) : QObject(parent) {}
    virtual ~Simulator() {}

    //! Loads file from path and initializes the simulation.
    virtual bool loadFile(const QString & path) = 0;
    
    /*!
     * Returns whether simulator is ready for simulation (initialization
     * was successful).
     */
    virtual bool isReady(void) const = 0;
    
    //! Loads saved trail from given file.
    virtual bool loadTrail(const QString & path) = 0;
    //! Saves the current run to specified trail file.
    virtual bool saveTrail(const QString & path) const = 0;

    //! Returns list of absolute paths of all files composing the current model.
    virtual const QStringList files(void) const = 0;

    //! Retrieves errors during initialization.
    virtual const SyntaxErrorList errors(void) = 0;
    
    //! Returns whether the system is running.
    virtual bool isRunning(void) const = 0;
    
    //! Determines whether the current state is not at the bottom of stack.
    virtual bool canUndo(void) const = 0;
    
    //! Determines whether the current state is not at the top of stack.
    virtual bool canRedo(void) const = 0;

    /*!
     * Gets simulator capabilities.
     * \see capChannelMutable
     * \see capProcessMutable
     * \see capVariableMutable
     */
    virtual int capabilities(void) const = 0;
    
    //! Gets the current depth of the stack.
    virtual int stackDepth(void) const = 0;
    //! Gets the position of the current state in the stack.
    virtual int currentState(void) const = 0;
    //! Determines whether the given state is accepting.
    virtual bool isAccepting(int state = -1) const = 0;
    //! Determines whether the given state is errorneous.
    virtual bool isErrorneous(int state = -1) const = 0;
    //! Returns the last found accepting cycle.
    virtual const QPair<int, int> acceptingCycle(void) const = 0;
    
    //! Gets the transition count in the given state.
    virtual int transitionCount(int state = -1) const = 0;
    //! Gets the ID of the transition which was used to advance to the given state.
    virtual int usedTransition(int state = -1) const = 0;
    /*!
     * Gets source information about a system transition.
     * \param tid Transition ID.
     * \param state Target state.
     * \return List of process transitions creating the requested system transition.
     */
    virtual const TransitionList transitionSource(int tid, int state = -1) const = 0;
    /*!
     * Gets short name of the system transition. This is usually composed of
     * process and transition IDs.
     * \param tid Transition ID.
     * \param state Target state.
     */
    virtual const QString transitionName(int tid, int state = -1) const = 0;
    /*!
     * Gets list of names of process transitions creating the the requested
     * system transition.
     * \param tid Transition ID.
     * \param state Target state.
     */
    virtual const QStringList transitionText(int tid, int state = -1) const = 0;
    /*!
     * Returns synchronisation details for the given system transition.
     * \param tid Transition ID.
     * \param state Target state.
     */
    virtual const SyncInfo transitionChannel(int tid, int state = -1) const  = 0;
    
    // channels
    //! Gets the number of buffered channels in a system.
    virtual int channelCount(void) const = 0;
    /*!
     * Gets the name of the specified channel.
     * \param cid Channel ID.
     */
    virtual const QString channelName(int cid) const = 0;
    /*!
     * Retrives the contents of the specified channel.
     * \param cid Channel ID.
     * \param state Target state.
     */
    virtual const QVariantList channelValue(int cid, int state = -1) const = 0;
    /*!
     * Replaces contents of the specified channel in the current state with
     * given values.
     * \param cid Channel ID.
     * \param val New contents.
     */
    virtual void setChannelValue(int cid, const QVariantList & val) = 0;
    
    //! Gets the number of processes in the system.
    virtual int processCount(void) const = 0;
    /*!
     * Gets the name of the specified process.
     * \param pid Process ID.
     */
    virtual const QString processName(int pid) const = 0;
    /*!
     * Gets names of the states in the specified process.
     * \param pid Process ID.
     */
    virtual const QStringList enumProcessStates(int pid) const = 0;
    /*!
     * Gets the value of the current state in the specified process.
     * \param pid Process ID.
     * \param state Target state.
     */
    virtual int processState(int pid, int state = -1) const = 0;
    /*!
     * Sets the process state to the provided value (in the current system state).
     * \param pid Process ID.
     * \param val New process state value.
     */
    virtual void setProcessState(int pid, int val) = 0;
    
    // variables
    /*!
     * Gets the number of variables in the given process.
     * \param pid Process ID.
     */
    virtual int variableCount(int pid) const = 0;
    /*!
     * Gets the name of the specified variable.
     * \param pid Process ID.
     * \param vid Variable ID.
     */
    virtual const QString variableName(int pid, int vid) const = 0;
    /*!
     * Gets the variable flags for the specified variable.
     * \param pid Process ID.
     * \param vid Variable ID.
     * \param sub Helps pinpoint the variable (in compound constructs).
     * \see varConst
     */
    virtual int variableFlags(int pid, int vid, const QVariantList * sub = NULL) const = 0;
    /*!
     * Retrieves the value of the specified variable.
     * \param pid Process ID.
     * \param vid Variable ID.
     * \param state Target state.
     */
    virtual const QVariant variableValue(int pid, int vid, int state = -1) const = 0;
    /*!
     * Set the value of the specified variable.
     * \param pid Process ID.
     * \param vid Variable ID.
     * \param val New value.
     */
    virtual void setVariableValue(int pid, int vid, const QVariant & val) = 0;
    
  public slots:
    //! Starts the simulation.
    virtual void start(void) = 0;
    //! Stops the simulation.
    virtual void stop(void) = 0;
    //! Moves one step back in the stack.
    virtual void undo(void) = 0;
    //! Moves one step forward in the stack.
    virtual void redo(void) = 0;
    /*!
     * Makes a step in the simulation.
     * \param id ID of the transition to be used.
     */
    virtual void step(int id) = 0;
    //! moves to the state in the stack at the given depth.
    virtual void backstep(int depth) = 0;

  signals:
    //! Sends a text message to the user
    void message(const QString & msg);

    //! Notifies of simulation start
    void started(void);
    
    //! Notifies of a simulation end
    void stopped(void);
    
    //! Notifies of a system state change
    void stateChanged(void);
};

/*!
 * The SimulatorProxy class is a wrapper around the Simulator class enhancing
 * it with locking: one object can temporarily prevent others from changing the
 * state of the simulator.
 */
class BASE_SHARED_EXPORT SimulatorProxy : public QObject
{
  Q_OBJECT
  
  public:
    SimulatorProxy(Simulator * sim, QObject * parent = NULL);
    ~SimulatorProxy();
    
    bool lock(QObject * owner);
    bool release(QObject * owner);
    
    //! Tells whether the simulator is locked
    bool isLocked(void) const {return lock_ != NULL;}
    
    //! Provides access to the const methods of the simulator
    // const functions can be directly accessed
    const Simulator * simulator(void) const {return sim_;}
    
    // non-const functions are delegated
    void setChannelValue(int cid, const QVariantList & val, QObject * owner = NULL);
    void setProcessState(int pid, int val, QObject * owner = NULL);
    void setVariableValue(int pid, int vid, const QVariant & val, QObject * owner = NULL);
    
  public slots:
    void start(void);
    void stop(void);
    void undo(QObject * owner = NULL);
    void redo(QObject * owner = NULL);
    void step(int id, QObject * owner = NULL);
    void backstep(int depth, QObject * owner = NULL);
    
  signals:
    //! Notifies of a lock change
    void locked(bool lock);
    
    //! Forwards Simulator::message
    void message(const QString & msg);

    //! Forwards Simulator::started
    void started(void);
    
    //! Forwards Simulator::stopped
    void stopped(void);
    
    //! Forwards Simulator::stateChanged
    void stateChanged(void);
    
  private:
    Simulator * sim_;
    QObject * lock_;
};

#endif
