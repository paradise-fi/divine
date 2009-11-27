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

#include <sstream>

#include <QFile>
#include <QMessageBox>

#include "dve_simulator.h"
#include "debug_system.h"

#include "divine/generator/common.h"

void errorHandler1(divine::error_vector_t &, const divine::ERR_throw_t)
{ 
  Q_ASSERT(DveSimulator::instance());
  DveSimulator::instance()->errorHandler();
}

void errorHandler2(divine::error_vector_t &, const divine::ERR_throw_t tp)
{
  Q_ASSERT(DveSimulator::instance());
  DveSimulator::instance()->errorHandler();
  throw tp;
}

void syntaxHandler1(divine::error_vector_t & err, const divine::ERR_throw_t)
{
}

void syntaxHandler2(divine::error_vector_t & err, const divine::ERR_throw_t tp)
{
  throw tp;
}

DveSimulator * DveSimulator::instance_ = NULL;

DveSimulator::DveSimulator(QObject * parent)
    : Simulator(parent), generator_(NULL), system_(NULL), state_(-1)
{
  Q_ASSERT(!instance_);
  instance_ = this;

  err_.set_silent(true);
  
  generator_ = new divine::generator::Allocator();
}

DveSimulator::~DveSimulator()
{
  clearSimulation();

  delete generator_;
  
  instance_ = NULL;
}

bool DveSimulator::openFile(const QString & fileName)
{
  clearSimulation();
  
  err_.set_push_callback(syntaxHandler1);
  err_.set_throw_callback(syntaxHandler2);
  
  dve_debug_system_t * newsys = new dve_debug_system_t(err_);
  newsys->setAllocator(generator_);
  
  fileName_ = fileName;
  
  if(newsys->read(fileName.toAscii().data()) != 0) {
    delete newsys;
    return false;
  }  

  err_.set_push_callback(errorHandler1);
  err_.set_throw_callback(errorHandler2);

  system_ = newsys;
  return true;
}

bool DveSimulator::loadTrail(const QString & fileName)
{
  QFile trail(fileName);
  if(!trail.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QMessageBox::warning(NULL, tr("DiVinE IDE"), tr("Unable to open trail '%1'!").arg(fileName));
    return false;
  }

  restart();

  QByteArray line;
  while(!trail.atEnd()) {
    line = trail.readLine();

    if(line.simplified().startsWith('#'))
      continue;

    int id = line.simplified().toInt();
    if(id <= 0 || id > transitionCount()) {
      QMessageBox::warning(NULL, tr("DiVinE IDE"), tr("Invalid trail!"));
      stop();
      return false;
    }
    
    // advance to the next state
    step(id - 1);
  }
  return true;
}

bool DveSimulator::saveTrail(const QString & fileName) const
{
  QFile trail(fileName);
  if(!trail.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QMessageBox::warning(NULL, tr("DiVinE IDE"), tr("Unable to save trail '%1'!").arg(fileName));
    return false;
  }
  
  trail.write("# from initial\n");
  
  for(int i = 0; i < stack_.size() - 1; ++i) {
    if(i == cycle_.second)
      trail.write("# cycle\n");
    
    if(i == cycle_.first)
      trail.write("# tail\n");
    
    trail.write(QString("%1\n").arg(usedTransition(i) + 1).toAscii().data());
  }
  return true;
}

const SyntaxErrorList DveSimulator::errors(void)
{
  QRegExp err_re("^(\\d+):(\\d+)-(\\d+):(\\d+)\\s*(.*)$");
  SyntaxErrorList res;
  
  int x1, y1, x2, y2;
  
  SyntaxError tmp_err;
  tmp_err.file = fileName_;
  
  for(int i = 0; i < err_.count(); ++i) {
    tmp_err.message = QString((*err_.string(i)).c_str()).simplified();
    tmp_err.block = QRect();
    
    if(err_re.exactMatch(tmp_err.message)) {
      y1 = err_re.capturedTexts().at(1).toInt();
      x1 = err_re.capturedTexts().at(2).toInt();
      y2 = err_re.capturedTexts().at(3).toInt();
      x2 = err_re.capturedTexts().at(4).toInt();

      tmp_err.block = QRect(x1 - 1, y1 - 1, x2 - x1, y2 - y1 + 1);
    }
    res.append(tmp_err);
  }
  err_.clear();
  
  return res;
}

int DveSimulator::traceDepth(void) const
{
  return stack_.size();
}

int DveSimulator::currentState(void) const
{
  return state_;
}

bool DveSimulator::isAccepting(int state) const
{
  if(!isRunning())
    return false;
  
  if(state == -1)
    state = state_;
  
  Q_ASSERT(system_);
  Q_ASSERT(state >= 0 && state < stack_.size());
  
  return system_->is_accepting(stack_[state]);
}

bool DveSimulator::isErrorneous(int state) const
{
  if(!isRunning())
    return false;
  
  if(state == -1)
    state = state_;
  
  Q_ASSERT(system_);
  Q_ASSERT(state >= 0 && state < stack_.size());
  
  return system_->is_erroneous(stack_[state]);
}

const QPair<int, int> DveSimulator::acceptingCycle(void) const
{
  return cycle_;
}

int DveSimulator::transitionCount(int state) const
{
  if(!isRunning())
    return 0;
  
  if(state == -1)
    state = state_;
  
  Q_ASSERT(system_);
  
  divine::size_int_t cnt;
  system_->get_enabled_trans_count(stack_[state], cnt);
  return cnt;
}

int DveSimulator::usedTransition(int state) const
{
  if(!isRunning())
    return -1;
  
  if(state == -1)
    state = state_;
  
  // already on top of stack
  if(state == stack_.size() - 1)
    return -1;
  
  divine::state_t succ;
  divine::size_int_t cnt;
  
  system_->get_enabled_trans_count(stack_[state], cnt);
  
  for(int i = 0; i < cnt; ++i) {
    system_->get_ith_succ(stack_[state], i, succ);

    if(stack_[state + 1] == succ)
      return i;
  }
  return -1;
}

const DveSimulator::TransitionList DveSimulator::transitionSource(int tid, int state) const
{
  if(!isRunning())
    return TransitionList();
  
  if(state == -1)
    state = state_;
 
  Q_ASSERT(system_);
  Q_ASSERT(tid >= 0 && tid < transitionCount(state));
  
  TransitionList res;
  TransitionPair pair;

  pair.first = fileName_;
  
  divine::dve_enabled_trans_t trans;
  system_->get_enabled_ith_trans(stack_[state], tid, trans);

  for(uint i = 0;i < trans.get_count(); ++i) {
    divine::dve_transition_t * ptr_dve =
      dynamic_cast<divine::dve_transition_t *>(trans[i]);

    Q_ASSERT(ptr_dve);

    int x1, x2, y1, y2;
    x1 = ptr_dve->get_source_first_col();
    x2 = ptr_dve->get_source_last_col();
    y1 = ptr_dve->get_source_first_line();
    y2 = ptr_dve->get_source_last_line();

    pair.second = QRect(x1 - 1, y1 - 1, x2 - x1, y2 - y1 + 1);
    res.append(pair);
  }
  return res;
}

const QString DveSimulator::transitionName(int tid, int state) const
{
  if(!isRunning())
    return QString();

  if(state == -1)
    state = state_;
 
  Q_ASSERT(system_);
  Q_ASSERT(tid >= 0 && tid < transitionCount(state));
    
  divine::dve_enabled_trans_t trans;
  system_->get_enabled_ith_trans(stack_[state], tid, trans);

  return QString(trans.to_string().c_str());
}

const QStringList DveSimulator::transitionText(int tid, int state) const
{
  if(!isRunning())
    return QStringList();
  
  if(state == -1)
    state = state_;
 
  Q_ASSERT(system_);
  Q_ASSERT(tid >= 0 && tid < transitionCount(state));
  
  QStringList res;
  
  divine::dve_enabled_trans_t trans;
  system_->get_enabled_ith_trans(stack_[state], tid, trans);
  
  for(uint i = 0;i < trans.get_count(); ++i) {
    divine::dve_transition_t * ptr_dve =
      dynamic_cast<divine::dve_transition_t *>(trans[i]);

    std::stringstream sstr;
    trans[i]->write(sstr);
      
    res.append(QString("%1: %2").arg(processName(ptr_dve->get_process_gid()), sstr.str().c_str()));
  }
  return res;
}

const SyncInfo DveSimulator::transitionChannel(int tid, int state) const
{
  if(!isRunning())
    return SyncInfo();
  
  if(state == -1)
    state = state_;
 
  Q_ASSERT(system_);
  Q_ASSERT(tid >= 0 && tid < transitionCount(state));
  
  divine::dve_enabled_trans_t trans;
  system_->get_enabled_ith_trans(stack_[state], tid, trans);
  
  int sender = -1, receiver = -1;
  QString channel;
  
  for(uint i = 0;i < trans.get_count(); ++i) {
    divine::dve_transition_t * ptr_dve =
      dynamic_cast<divine::dve_transition_t *>(trans[i]);
    
    if(!ptr_dve->is_without_sync()) {
      if(channel.isEmpty())
        channel = ptr_dve->get_sync_channel_name();
        
      if(ptr_dve->is_sync_exclaim()) {
        sender = ptr_dve->get_process_gid();
      } else if(ptr_dve->is_sync_ask()) {
        receiver = ptr_dve->get_process_gid();
      }
    }
  }
  
  return SyncInfo(sender, receiver, channel);
}

int DveSimulator::channelCount(void) const
{
  if(!isRunning())
    return 0;
  
  Q_ASSERT(system_);
  
  return system_->get_typed_channel_count();
}

const QString DveSimulator::channelName(int cid) const
{
  if(!isRunning())
    return QString();
  
  Q_ASSERT(system_);
  Q_ASSERT(cid >= 0 && cid < channelCount());
  
  divine::dve_symbol_table_t * symbols = system_->get_symbol_table();
  divine::dve_symbol_t * smb = symbols->get_channel(system_->get_channel_gid(cid));

  return smb->get_name();
}

const QVariantList DveSimulator::channelValue(int cid, int state) const
{
  if(!isRunning())
    return QVariantList();

  if(state == -1)
    state = state_;
  
  Q_ASSERT(state >= 0 && state < stack_.size());
  Q_ASSERT(system_);
  
  divine::dve_symbol_table_t * symbols = system_->get_symbol_table();
  uint gid = system_->get_channel_gid(cid);
  
  divine::dve_symbol_t * smb = symbols->get_channel(gid);
  
  QVariantList res;
  
  for(int i = 0; i < system_->get_channel_contents_count(stack_[state], gid); ++i) {
    if(smb->get_channel_item_count() == 1) {
      res.append(system_->get_channel_value(stack_[state], gid, i));
    } else {
      QVariantList tmp;
      
      for(int j = 0; j < smb->get_channel_item_count(); ++j) {
	tmp.append(system_->get_channel_value(stack_[state], gid, i, j));
      }
      res.append(tmp);
    }
  }
  return res;
}

// not supported
void DveSimulator::setChannelValue(int cid, const QVariantList & val)
{
}

int DveSimulator::processCount(void) const
{
  if(!isRunning())
    return 0;
  
  Q_ASSERT(system_);
  
  return system_->get_process_count();
}

const QString DveSimulator::processName(int pid) const
{
  if(!isRunning())
    return QString();
  
  Q_ASSERT(system_);
  Q_ASSERT(pid >= 0 && pid < system_->get_process_count());

  divine::dve_symbol_table_t * symbols = system_->get_symbol_table();
  divine::dve_symbol_t * smb = symbols->get_process(pid);
  
  Q_ASSERT(smb && smb->get_name());
  
  return smb->get_name();
}

const QStringList DveSimulator::enumProcessStates(int pid) const
{
  if(!isRunning())
    return QStringList();
  
  Q_ASSERT(system_);
  Q_ASSERT(pid >= 0 && pid < system_->get_process_count());
  
  divine::dve_symbol_table_t * symbols = system_->get_symbol_table();
  divine::dve_symbol_t * smb;
  
  divine::dve_process_t * proc = dynamic_cast<divine::dve_process_t*>(system_->get_process(pid));
  uint gid;

  QStringList res;
  
  for(uint i = 0; i < proc->get_state_count(); ++i) {
    smb = symbols->get_state(proc->get_state_gid(i));

    Q_ASSERT(smb && smb->get_name());

    res.append(smb->get_name());
  }
  return res;
}

int DveSimulator::processState(int pid, int state) const
{
  if(!isRunning())
    return 0;
  
  if(state == -1)
    state = state_;
  
  Q_ASSERT(system_);
  Q_ASSERT(pid >= 0 && pid < system_->get_process_count());
  Q_ASSERT(state >= 0 && state < stack_.size());
  
  return system_->get_state_of_process(stack_[state], pid);
}

// not supported
void DveSimulator::setProcessState(int pid, int val)
{
}

int DveSimulator::variableCount(int pid) const
{
  if(!isRunning())
    return 0;
  
  Q_ASSERT(system_);
  
  if(pid == globalId) {
    return system_->get_global_variable_count();
  } else {
    Q_ASSERT(pid >= 0 && pid < system_->get_process_count());
    
    divine::dve_process_t * proc = dynamic_cast<divine::dve_process_t*>(system_->get_process(pid));
    return proc->get_variable_count();
  }
}

const QString DveSimulator::variableName(int pid, int vid) const
{
  if(!isRunning())
    return QString();

  divine::dve_symbol_t * smb = findVariableSymbol(pid, vid);
  return smb->get_name();
}

int DveSimulator::variableFlags(int pid, int vid, const QVariantList *) const
{
  if(!isRunning())
    return 0;

  divine::dve_symbol_t * smb = findVariableSymbol(pid, vid);
  
  if(smb->is_const())
    return varConst;

  return 0;
}

const QVariant DveSimulator::variableValue(int pid, int vid, int state) const
{
  if(!isRunning())
    return QVariant();
  
  if(state == -1)
    state = state_;
  
  Q_ASSERT(state >= 0 && state < stack_.size());
  
  divine::dve_symbol_t * smb = findVariableSymbol(pid, vid);
  divine::all_values_t value;
  
  if(smb->is_vector()) {
    QVariantList res;
    
    for(int i = 0; i < smb->get_vector_size(); ++i) {
      value = system_->get_var_value(stack_[state], smb->get_gid(), i);
      res.append(value);
    }
    return QVariant(res);
  } else {
    value = system_->get_var_value(stack_[state], smb->get_gid(), 0);
    return QVariant(value);
  }
}

void DveSimulator::setVariableValue(int pid, int vid, const QVariant & val)
{
  if(!isRunning())
    return;
  
  divine::dve_symbol_t * smb = findVariableSymbol(pid, vid);
  if(smb->is_const())
    return;
  
  Q_ASSERT(!stack_.isEmpty());
  
  if(smb->is_vector()) {
    QVariantList tmp = val.toList();
    for(int i = 0; i < tmp.size(); ++i) {
      system_->set_var_value(stack_.last(), smb->get_gid(), tmp[i].toInt(), i);
    }
  } else {
    system_->set_var_value(stack_.last(), smb->get_gid(), val.toInt(), 0);
  }
 
  // invalide the top of the stack
  trimStack();

  emit stateChanged();
}

void DveSimulator::errorHandler(void)
{
  QString str;

  for(int i = 0; i < err_.count(); ++i) {
    str = (*err_.string(i)).c_str();
    emit message(str);
  }
  err_.clear();
}

void DveSimulator::start(void)
{
  if(!system_)
    return;

  if(isRunning())
    clearStack();
  
  divine::state_t first(system_->get_initial_state());

  stack_.append(first);
  state_ = 0;
  
  // in case the first state is accepting self-cycle
  updateAcceptingCycle();
  
  emit started();
  emit stateReset();
}

void DveSimulator::restart(void)
{
  if(!isRunning())
    return;

  start();
}

void DveSimulator::stop(void)
{
  if(!isRunning())
    return;

  clearStack();
  
  emit stopped();
}

void DveSimulator::undo(void)
{
  if(canUndo())
    backtrace(state_ - 1);
}

void DveSimulator::redo(void)
{
  if(canRedo())
    backtrace(state_ + 1);
}

void DveSimulator::step(int id)
{
  if(!isRunning())
    return;
  
  Q_ASSERT(system_);
  Q_ASSERT(id >= 0 && id < transitionCount());

  divine::state_t next;
  system_->get_ith_succ(stack_[state_], id, next);

  // advance "in" stack, if possible
  if(state_ < stack_.size() - 1 && stack_[state_ + 1] == next) {
    backtrace(state_ + 1);
    return;
  }

  trimStack();
  
  stack_.append(next);
  state_ = stack_.size() - 1;
  
  updateAcceptingCycle(state_);

  emit stateChanged();
}

void DveSimulator::backtrace(int state)
{
  if(!system_ || !isRunning())
    return;
  
  Q_ASSERT(state >= 0 && state < stack_.size());

  state_ = state;

  emit stateChanged();
}

void DveSimulator::clearSimulation(void)
{
  clearStack();
  
  delete system_;
  system_ = NULL;
}

void DveSimulator::clearStack(void)
{
  cycle_ = qMakePair(-1, -1);
  
  foreach(divine::state_t state, stack_) {
    generator_->delete_state(state);
  }
  stack_.clear();
  state_ = -1;
}

void DveSimulator::trimStack(void)
{
  if(state_ == stack_.size() - 1)
    return;

  for(int i = state_ + 1; i < stack_.size(); ++i) {
    generator_->delete_state(stack_[i]);
  }
  stack_.erase(stack_.begin() + state_ + 1, stack_.end());
  
  if(cycle_.first > state_)
    updateAcceptingCycle();
}

divine::dve_symbol_t * DveSimulator::findVariableSymbol(int pid, int vid) const
{
  Q_ASSERT(system_);

  divine::dve_symbol_table_t * symbols = system_->get_symbol_table();
  
  if(pid == globalId) {
    Q_ASSERT(vid >= 0 && vid < system_->get_global_variable_count());

    return symbols->get_variable(system_->get_global_variable_gid(vid));
  } else {
    Q_ASSERT(pid >= 0 && pid < system_->get_process_count());
    
    divine::dve_process_t * proc = dynamic_cast<divine::dve_process_t*>(system_->get_process(pid));
    
    Q_ASSERT(vid >= 0 && vid < proc->get_variable_count());
    
    return symbols->get_variable(proc->get_variable_gid(vid));
  }
}

void DveSimulator::updateAcceptingCycle(void)
{
  // reset cycle
  cycle_ = qMakePair(-1, -1);

  // newer states have bigger priority
  for(int i = stack_.size() - 1; i >= 0; --i) {
    updateAcceptingCycle(i);
    
    if(cycle_ != qMakePair(-1, -1))
      break;
  }
}

void DveSimulator::updateAcceptingCycle(int state)
{
  Q_ASSERT(state >= 0 && state < stack_.size());
  
  bool accepting = isAccepting(state);
  
  for(int i = state - 1; i >= 0; --i) {
    if(isAccepting(i))
      accepting = true;
    
    if(accepting && stack_[i] == stack_[state]) {
      cycle_ = qMakePair(state, i);
      break;
    }
  }
}
