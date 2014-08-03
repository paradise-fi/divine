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

#include <divine/utility/statistics.h>
#include <divine/instances/select.h>

#include <QMessageBox>

#include "mainForm.h"
#include "divineTools.h"

namespace divine {
namespace gui {

//
// DivineStreambuf
//
int DivineStreambuf::sync()
{
  int r = std::stringbuf::sync();

  while ( str().find( '\n' ) != std::string::npos ) {
    std::string line( str(), 0, str().find( '\n' ) );
    emit message(line.c_str());
    str( std::string( str(), line.length() + 1, str().length() ) );
  }
  return r;
}

//
// DivineOutput
//
DivineOutput::DivineOutput(QObject * parent)
  : QObject(parent), buf_(new DivineStreambuf(this)), stream_(buf_)
{
  connect(buf_, SIGNAL(message(QString)), this, SIGNAL(message(QString)));
}

std::ostream & DivineOutput::statistics()
{
  return stream_;
}

std::ostream & DivineOutput::progress()
{
  return stream_;
}

std::ostream & DivineOutput::debug()
{
  // spamfilter :)
  static struct : std::streambuf {} buf;
  static std::ostream null(&buf);
  return null;
}

//
// DivineLock
//
DivineLock::DivineLock(MainForm * root, DivineRunner * runner, QObject * parent)
  : QObject(parent), root_(root), runner_(runner), doc_(NULL)
{
}

DivineLock::~DivineLock()
{
}

bool DivineLock::isLocked(AbstractDocument * document) const
{
  return doc_ == document;
}

void DivineLock::lock(AbstractDocument * document)
{
  Q_ASSERT(!doc_);
  Q_ASSERT(!root_->isLocked());

  doc_ = document;
  root_->lock(this);
}

bool DivineLock::maybeRelease()
{
  if(!doc_)
    return true;

  int ret = QMessageBox::warning(root_, tr("DiVinE IDE"),
                                 tr("%1 is in progress.\n"
                                    "Do you want to abort it?").arg(process()),
                                 QMessageBox::Yes | QMessageBox::Default,
                                 QMessageBox::No | QMessageBox::Escape);

  if (ret != QMessageBox::Yes)
    return false;

  runner_->abort();
  runner_->wait();
  return true;
}

void DivineLock::forceRelease()
{
  Q_ASSERT(doc_);
  Q_ASSERT(root_->isLocked(this));

  doc_ = NULL;
  root_->release(this);
}

QString DivineLock::process() const
{
  return process_.isEmpty() ? tr("An operation") : process_;
}

void DivineLock::setProcess(const QString & name)
{
  process_ = name;
}

//
// DivnineRunner
//
DivineRunner::DivineRunner(QObject * parent)
  : QThread(parent)
{
}

void DivineRunner::init(const Meta & meta)
{
  meta_ = meta;
}

const QVector<int> DivineRunner::iniTrail() const
{
  return QVector< int >::fromStdVector( meta_.result.iniTrail );
}

const QVector<int> DivineRunner::cycleTrail() const
{
  return QVector< int >::fromStdVector( meta_.result.cycleTrail );
}

void DivineRunner::run()
{
  auto alg = divine::select(meta_);

  Q_ASSERT(alg);

  TrackStatistics::global().setup(meta_);

  if(meta_.output.statistics)
    TrackStatistics::global().start();

  alg->run();
  meta_ = alg->meta();
}

void DivineRunner::abort()
{
// TODO: wait until early termination is implemented
//   a_->abort();
}

}
}
