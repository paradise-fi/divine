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
#include <fstream>
#include <limits>

#include <divine/dve/interpreter.h>

#include "abstractDocument.h"
#include "mainForm.h"
#include "dveSimulator.h"

#include <QMessageBox>
#include <QHash>
#include <QFile>
#include <QTextStream>

namespace divine {
namespace gui {

namespace {
  // creates EvalContext to be used with this Blob
  inline dve::EvalContext context(Blob b)
  {
    dve::EvalContext ctx;
    ctx.mem = b.data();
    return ctx;
  }

  inline void validateIndex(int & index, const QList<Blob> & stack)
  {
    Q_ASSERT(index >= 0 && index < stack.count());
  }

  // creates var types for the symbol table (if necessary)
  QString createVarType(QHash<QString, SymbolType*> & types, dve::SymContext::Item & item)
  {
    QString desc;

    if(item.width == 2)
      desc = "int";
    else
      desc = "byte";

    if(!item.is_array)
      return desc;

    QString name = QString("%1[%2]").arg(desc).arg(item.array);

    if(types.contains(name))
      return name;

    ArrayType * type = new ArrayType();
    type->id = name;
    type->name = name;
    type->subtype = desc;
    type->size = item.array;
    types.insert(type->id, type);
    return type->id;
  }

  uint findProcess(const dve::System * system, dve::SymId id)
  {
    for(uint i=0; i < system->processes.size(); ++i) {
      if(id == system->processes[i].id.id)
        return i;
    }
    // die!
    Q_ASSERT(0);
    return 0;
  }

  // NOTE: Workaround! States in symtab do not correlate to the actual state value
  QStringList sortStates(const dve::SymTab::Tab & tab) {
    typedef QPair<dve::SymId, QString> StatePair;
    QList<StatePair> states;

    foreach(dve::SymTab::Tab::value_type itr, tab) {
      states.append(StatePair(itr.second, itr.first.c_str()));
    }
    qSort(states);

    // retrieve only state names
    QStringList names;
    foreach(const StatePair & itr, states) {
      names.append(itr.second);
    }
    return names;
  }

}

//
// Dve simulator
//
DveSimulator::DveSimulator(QObject * parent)
  : AbstractSimulator(parent), system_(NULL)
{
}

DveSimulator::~DveSimulator()
{
  stop();
  closeSystem();
}

bool DveSimulator::loadDocument(AbstractDocument * doc)
{
  return loadSystem(doc->path());
}

bool DveSimulator::loadSystem(const QString & path)
{
  stop();

  // try to load the dve from file
  std::ifstream file;
  file.open(path.toAscii().data());

  dve::IOStream stream(file);
  dve::Lexer<dve::IOStream> lexer(stream);
  dve::Parser::Context ctx(lexer);
  try {
    dve::parse::System ast(ctx);
    system_ = new dve::System(ast);
  } catch (...) {
    std::stringstream what;
    ctx.errors(what);

    emit message(what.str().c_str());
    QMessageBox::critical(NULL, tr("DiVinE IDE"), tr("Source contains errors."));
    return false;
  }
  file.close();

  path_ = path;
  sources_.clear();

  createSymbolTable();
  return true;
}

void DveSimulator::closeSystem()
{
  clearSymbolTable();

  delete system_;
  system_ = NULL;
}

int DveSimulator::stateCount() const
{
  return stack_.count();
}

bool DveSimulator::isAccepting(int state)
{
  validateIndex(state, stack_);

  dve::EvalContext ctx = context(stack_.at(state));
  return system_->accepting(ctx);
}

bool DveSimulator::isValid(int state)
{
  validateIndex(state, stack_);

  dve::ErrorState err;
  system_->symtab.lookup(dve::NS::Flag, "Error").deref(stack_[state].data(), 0, err);

  return err.error == 0;
}

QPair<int, int> DveSimulator::findAcceptingCycle()
{
  Q_ASSERT(system_);

  int from;

  // newer states have bigger priority
  for(int to = stack_.size() - 1; to >= 0; --to) {
    if((from = findAcceptingCycle(to)) >= 0)
      return qMakePair(from, to);
  }
  return qMakePair(-1, -1);
}

int DveSimulator::transitionCount(int state)
{
  validateIndex(state, stack_);
  updateTransitions(state);
  return transitions_.count();
}

const Transition * DveSimulator::transition(int index, int state)
{
  validateIndex(state, stack_);
  updateTransitions(state);

  Q_ASSERT(index >= 0 && index < transitions_.count());

  return &transitions_[index];
}

QList<const Communication*> DveSimulator::communication(int index, int state)
{
  validateIndex(state, stack_);
  updateTransitions(state);

  Q_ASSERT(index >= 0 && index < transitions_.count());
  return QList<const Communication*>() << &transitions_[index].sync;
}

QList<const Transition*> DveSimulator::transitions(int state)
{
  validateIndex(state, stack_);
  updateTransitions(state);

  QList<const Transition*> res;
  foreach(const DveTransition & tr, transitions_) {
    res.append(&tr);
  }
  return res;
}

// find the transition used to move from `state` to `state + `
int DveSimulator::usedTransition(int state)
{
  validateIndex(state, stack_);

  // not possible for the last state
  if(state == stack_.count() - 1)
    return -1;

  Blob srcBlob = stack_[state];
  dve::EvalContext srcCtx = context(srcBlob);

  // find our continuation
  dve::System::Continuation cont = system_->enabled(srcCtx, dve::System::Continuation());
  for(int i=0; system_->valid(srcCtx, cont); cont = system_->enabled(srcCtx, cont), ++i) {
    // create new state
    Blob dstBlob = newBlob();
    srcBlob.copyTo(dstBlob);
    dve::EvalContext dstCtx = context(dstBlob);

    system_->apply(dstCtx, cont);

    // check equality
    if(dstBlob == stack_[state + 1])
      return i;
  }
  return -1;
}

const SymbolType * DveSimulator::typeInfo(const QString & id) const
{
  QHash<QString, SymbolType*>::const_iterator itr = types_.find(id);
  return itr != types_.end() ? itr.value() : NULL;
}

const Symbol * DveSimulator::symbol(const QString & id, int state)
{
  validateIndex(state, stack_);

  QHash<QString, DveSymbol*>::const_iterator itr = symbols_.find(id);
  return itr != symbols_.end() ? itr.value() : NULL;
}

QStringList DveSimulator::topLevelSymbols(int state)
{
  return topLevel_;
}

QVariant DveSimulator::symbolValue(const QString & symbol, int state)
{
  validateIndex(state, stack_);

  QHash<QString, DveSymbol*>::iterator itr = symbols_.find(symbol);
  if(itr == symbols_.end())
    return QVariant();

  Blob b = stack_[state];
  dve::EvalContext ctx = context(b);

  DveSymbol * smb = itr.value();
  Q_ASSERT(smb);

  SymbolType * type;

  Q_ASSERT(types_.contains(smb->type));
  type = types_.find(smb->type).value();

  QVariant value;

  if(dynamic_cast<ArrayType*>(type)) {
    dve::Symbol sym = dve::Symbol(system_->symtab.context, smb->dveId);

    QVariantList vlist;
    for(int i = 0; i < sym.item().array; ++i)
      vlist.append(QVariant(sym.deref(ctx.mem, i)));
    value.setValue(vlist);
  // lists don't have values but children
  } else if(dynamic_cast<ListType*>(type)) {
  // POD
  } else if(dynamic_cast<PODType*>(type)) {
    dve::Symbol sym = dve::Symbol(system_->symtab.context, smb->dveId);
    value.setValue(sym.deref(ctx.mem));
  // process
  } else {
    dve::Symbol sym = dve::Symbol(system_->symtab.context, smb->dveId);
    value.setValue(sym.deref(ctx.mem));
  }
  return value;
}

bool DveSimulator::setSymbolValue
(const QString & symbol, const QVariant & value, int state)
{
  validateIndex(state, stack_);

  // TODO: not implemented yet
  return false;
}

void DveSimulator::start()
{
  Q_ASSERT(stack_.isEmpty());

  if(!system_)
    return;

  Blob b = newBlob();
  dve::EvalContext ctx = context(b);
  system_->initial(ctx);

  stack_.append(b);
}

void DveSimulator::stop()
{
  trimStack(-1);
}

void DveSimulator::trimStack(int topIndex)
{
  if(topIndex < 0)
    topIndex = -1;  // -1 should clear the whole stack

  while(stack_.count() > topIndex + 1) {
    stack_.last().free();
    stack_.removeLast();
  }
}

void DveSimulator::step(int id)
{
  Q_ASSERT(id >= 0);

  Blob b = stack_.last();
  dve::EvalContext ctx = context(b);

  // find our continuation
  dve::System::Continuation cont = system_->enabled(ctx, dve::System::Continuation());
  Q_ASSERT(system_->valid(ctx, cont));

  for(int i = 0; i < id; ++i) {
    cont = system_->enabled(ctx, cont);
    Q_ASSERT(system_->valid(ctx, cont));
  }

  // create new state
  b = newBlob();
  stack_.last().copyTo(b);
  ctx = context(b);

  // yay!
  system_->apply(ctx, cont);

  stack_.append(b);
  emit stateChanged(stack_.count() - 1);
}

Blob DveSimulator::newBlob()
{
  Blob b = Blob(stateSize());
  b.clear();
  return b;
}

// TODO: move to dve::iterpreter? as system::stateSize maybe?
int DveSimulator::stateSize()
{
  Q_ASSERT(system_);
  return system_->symtab.context->offset;
}

int DveSimulator::findAcceptingCycle(int to)
{
  bool accepting = isAccepting(to);

  for(int from = to - 1; from >= 0; --from) {
    if(isAccepting(from))
      accepting = true;

    if(accepting && stack_[from] == stack_[to])
      return from;
  }
  return -1;
}

void DveSimulator::updateTransitions(int state)
{
  if(state == bufferedState_)
    return;

  Blob b = stack_[state];
  dve::EvalContext ctx = context(b);

  int count = 0;
  transitions_.clear();

  dve::System::Continuation cont = system_->enabled(ctx, dve::System::Continuation());
  for(; system_->valid(ctx, cont); cont = system_->enabled(ctx, cont)) {
    dve::Transition tr = system_->processes[cont.process].transition(ctx, cont.transition);

    DveTransition trans;
    QStringList descs;
    QRect src;

    // initiator
    src = QRect(QPoint(tr.parse.from.token.position.column,
                       tr.parse.from.token.position.line),
                QPoint(tr.parse.end.position.column,
                       tr.parse.end.position.line));
    trans.source.append(src);
    descs.append(QString("%1 : %2").arg(
      symbols_[reverse_[system_->processes[cont.process].id.id]]->name).arg(
      getSourceBlock(src)));

    // synced process
    if(tr.sync) {
      trans.sync.from = reverse_[tr.process.id];
      trans.sync.to = reverse_[tr.sync->process.id];
      trans.sync.medium = reverse_[tr.sync_channel.id];

      src = QRect(QPoint(tr.sync->parse.from.token.position.column,
                         tr.sync->parse.from.token.position.line),
                  QPoint(tr.sync->parse.end.position.column,
                         tr.sync->parse.end.position.line));
      trans.source.append(src);
      descs.append(QString("%1 : %2").arg(
        symbols_[trans.sync.to]->name).arg(
        getSourceBlock(src)));
    }

    // property process
    if(system_->property) {
      dve::Transition tr = system_->property->transition(ctx, cont.property);

      src = QRect(QPoint(tr.parse.from.token.position.column,
                         tr.parse.from.token.position.line),
                  QPoint(tr.parse.end.position.column,
                         tr.parse.end.position.line));
      trans.source.append(src);
      descs.append(QString("%1 : %2").arg(
        symbols_[reverse_[system_->property->id.id]]->name).arg(
        getSourceBlock(src)));
    }

    trans.description = descs.join("\n");

    transitions_.append(trans);
  }
}

// the ugly
// TODO: refactor to smaller pieces
void DveSimulator::createSymbolTable()
{
  const dve::SymTab::Tab * tab;
  const dve::SymTab * sub;

  // channel type
  ListType * channelType = new ListType();
  channelType->id = "c";
  channelType->name = "channel";
  types_.insert(channelType->id, channelType);

  // pod types
  PODType * podType = new PODType();
  podType->id = "byte";
  podType->name = "byte";
  podType->podType = PODType::Integer;
  podType->min = std::numeric_limits<quint8>::min();
  podType->max = std::numeric_limits<quint8>::max();
  types_.insert(podType->id, podType);

  podType = new PODType();
  podType->id = "int";
  podType->name = "int";
  podType->podType = PODType::Integer;
  podType->min = std::numeric_limits<qint16>::min();
  podType->max = std::numeric_limits<qint16>::max();
  types_.insert(podType->id, podType);

  // channels
  tab = &system_->symtab.tabs[dve::NS::Channel];
  foreach(dve::SymTab::Tab::value_type itr, *tab) {
    DveSymbol * sym = new DveSymbol();
    sym->id = QString("&%1").arg(itr.first.c_str());
    sym->type = channelType->id;
    sym->name = itr.first.c_str();
    sym->icon = MainForm::loadIcon(":/icons/code/function");
    sym->parent = QString();
    sym->flags = Symbol::ReadOnly;        // not supported yet
    sym->dveId = itr.second;

    symbols_.insert(sym->id, sym);
    topLevel_.append(sym->id);
    reverse_.insert(sym->dveId, sym->id);
  }

  // global variables
  tab = &system_->symtab.tabs[dve::NS::Variable];
  foreach(dve::SymTab::Tab::value_type itr, *tab) {
    dve::SymContext::Item item = dve::Symbol(system_->symtab.context, itr.second).item();
    DveSymbol * sym = new DveSymbol();
    sym->id = QString("$%1").arg(itr.first.c_str());
    sym->type = createVarType(types_, item);
    sym->name = itr.first.c_str();
    sym->icon = MainForm::loadIcon(":/icons/code/variable");
    sym->parent = QString();
    sym->flags = Symbol::ReadOnly;        // not supported yet
    sym->dveId = itr.second;

    symbols_.insert(sym->id, sym);
    topLevel_.append(sym->id);
  }

  // processes
  tab = &system_->symtab.tabs[dve::NS::Process];
  foreach(dve::SymTab::Tab::value_type pitr, *tab) {
    // find tab of this child
    sub = system_->symtab.child(dve::Symbol(system_->symtab.context, pitr.second));

    // create process type
    OptionType * type = new OptionType();
    type->id = QString("p%1").arg(pitr.second);
    type->name = "process";
    type->options = sortStates(sub->tabs[dve::NS::State]);

    types_.insert(type->id, type);

    // create process symbol
    DveSymbol * sym = new DveSymbol();
    sym->id = QString("@%1").arg(pitr.first.c_str());
    sym->type = type->id;
    sym->name = pitr.first.c_str();
    sym->icon = MainForm::loadIcon(":/icons/code/class");
    sym->parent = QString();
    sym->flags = Symbol::ReadOnly;        // not supported yet
    sym->dveId = pitr.second;

    // mark property processes
    if(system_->property && system_->property->id.id == pitr.second)
      sym->id = "~" + sym->id;

    symbols_.insert(sym->id, sym);
    reverse_.insert(sym->dveId, sym->id);
    topLevel_.append(sym->id);

    // being eidget friendly
    qSort(topLevel_);

    // local variables
    foreach(dve::SymTab::Tab::value_type itr, sub->tabs[dve::NS::Variable]) {
      dve::SymContext::Item item = dve::Symbol(sub->context, itr.second).item();
      DveSymbol * vsym = new DveSymbol();
      vsym->id = QString("@%1$%2").arg(pitr.first.c_str(), itr.first.c_str());
      vsym->id = QString::number(itr.second);
      vsym->type = createVarType(types_, item);
      vsym->name = itr.first.c_str();
      vsym->icon = MainForm::loadIcon(":/icons/code/variable");
      vsym->parent = sym->id;
      sym->flags = Symbol::ReadOnly;        // not supported yet
      vsym->dveId = itr.second;

      sym->children.append(vsym->id);
      symbols_.insert(vsym->id, vsym);
    }
  }
}

void DveSimulator::clearSymbolTable()
{
  foreach(SymbolType * type, types_) {
    delete type;
  }
  types_.clear();

  foreach(Symbol * sym, symbols_) {
    delete sym;
  }
  symbols_.clear();
  reverse_.clear();
  topLevel_.clear();
}

QString DveSimulator::getSourceBlock(const QRect & rect)
{
  // if already loaded - return immediately
  QHash<QRect, QString>::iterator itr = sources_.find(rect);
  if(itr != sources_.end())
    return itr.value();

  // otherwise load from file
  QFile file(path_);
  if (!file.open(QFile::ReadOnly | QFile::Text))
    return QString();

  QTextStream in(&file);
  QString text;

  int line = 1;

  // find the first line
  for(; line < rect.top(); ++line) {
    file.readLine();
  }
  // skip the offset
  file.read(rect.left() - 1);
  int column = rect.left() - 1;

  // read the requested block in a similar way
  for(; line < rect.bottom(); ++line, column = 0) {
    text += file.readLine();
  }
  text += file.read(rect.right() - column);

  text = text.simplified();

  sources_[rect] = text;
  return text;
}

}
}
