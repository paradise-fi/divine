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

#ifndef ABSTRACT_DOCUMENT_H_
#define ABSTRACT_DOCUMENT_H_

#include <QObject>
#include <QStringList>

namespace divine {
namespace gui {

class AbstractEditor;
  
class AbstractDocument : public QObject
{
  Q_OBJECT
  
  public:
    AbstractDocument(QObject * parent = NULL) : QObject(parent) {}
    virtual ~AbstractDocument() {}

    // identifies the document
    virtual QString path() const = 0;
    
    virtual int viewCount() const = 0;
    virtual QStringList views() const = 0;
    
    virtual QString mainView() const = 0;
    
    virtual AbstractEditor * editor(const QString & view) = 0;
    
    virtual void openView(const QString & view) = 0;
    
    // file operations
    virtual bool openFile(const QString & path) = 0;
};

}
}

#endif
