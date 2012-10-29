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

#ifndef DIVINE_TOOLS_H_
#define DIVINE_TOOLS_H_

#include <divine/algorithm/common.h>

#include <QThread>
#include <QObject>

#include "abstractToolLock.h"

namespace divine {
namespace gui {

class MainForm;
class AbstractDocument;

class DivineRunner;

class DivineStreambuf : public QObject, public std::stringbuf
{
  Q_OBJECT

  public:
    DivineStreambuf(QObject * parent) : QObject(parent) {}

  signals:
    void message(const QString & msg);

  protected:
    int sync();
};

class DivineOutput : public QObject, public divine::Output
{
  Q_OBJECT

  public:
    DivineOutput(QObject * parent);

    std::ostream & statistics();
    std::ostream & progress();
    std::ostream & debug();

  signals:
    void message(const QString & msg);

  private:
    DivineStreambuf * buf_;
    std::ostream stream_;
};

class DivineLock : public QObject, public AbstractToolLock
{
  Q_OBJECT

  public:
    DivineLock(MainForm * root, DivineRunner * runner, QObject * parent);
    ~DivineLock();

    bool isLocked(AbstractDocument * document) const;

    void lock(AbstractDocument * document);

    bool maybeRelease();
    void forceRelease();

    // how to call this whole thing?
    QString process() const;
    void setProcess(const QString & name);

  private:
    MainForm * root_;
    DivineRunner * runner_;
    AbstractDocument * doc_;

    QString process_;
};

class DivineRunner : public QThread
{
  Q_OBJECT

  public:
    DivineRunner(QObject * parent);

    void init(const Meta & meta);
    const Meta & meta() const { return meta_; }

    void run();

  public slots:
    void abort();

  private:
    Meta meta_;
    algorithm::Algorithm * a_;
};

}
}

#endif
