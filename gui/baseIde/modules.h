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

#ifndef MODULES_H_
#define MODULES_H_

#include <QObject>
#include <QString>
#include <QIcon>

namespace divine {
namespace gui {

class MainForm;
class AbstractDocument;
class AbstractSimulator;

/*!
 * The AbstractDocumentFactory classes create instances of documents, which
 * might represent various filetypes or models, and are responsible for
 * maintaing all editor views.
 */
class AbstractDocumentFactory : public QObject
{
    Q_OBJECT

  public:
    AbstractDocumentFactory(QObject * parent = NULL) : QObject(parent) {}

    // document information
    virtual QString name() const = 0;
    virtual QString suffix() const = 0;

    virtual QIcon icon() const {return QIcon();}
    virtual QString description() const {return QString();}
    virtual QString filter() const {return name();}

    /*!
     * This function creates a new instance of the document class.
     * \param parent Parent object.
     */
    virtual AbstractDocument * create(MainForm * root) const = 0;
};

/*!
 * The AbstractSimulatorFactory classes provide simulator instances for
 * given files.
 */
class AbstractSimulatorFactory : public QObject
{
    Q_OBJECT

  public:
    AbstractSimulatorFactory(QObject * parent = NULL) : QObject(parent) {}

    virtual QString suffix() const = 0;

    /*!
     * This function creates a new instance of the simulator.
     * \param parent Parent object.
     */
    virtual AbstractSimulator * create(MainForm * root) const = 0;
};

/*!
 * The AbstractModule class provides interface for all modules.
 */
class AbstractModule
{
  public:
    virtual ~AbstractModule() {};

    /*!
     * This function installs the plugin to the program.
     * \param root Application's main window.
     */
    virtual void install(MainForm * root) = 0;
};

}
}

#endif
