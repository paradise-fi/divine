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

#ifndef TEXT_DOCUMENT_H_
#define TEXT_DOCUMENT_H_

#include "abstractDocument.h"

namespace divine {
namespace gui {

class MainForm;
class TextEditor;

class TextDocument : public AbstractDocument
{
    Q_OBJECT

  public:
    TextDocument(MainForm * root);
    ~TextDocument();

    QString path() const;

    int viewCount() const;
    QStringList views() const;

    QString mainView() const;

    AbstractEditor * editor(const QString & view);

    void openView(const QString & view);

    bool openFile(const QString & path);

  protected:
    TextEditor * editor_;
    MainForm * root_;
};

}
}

#endif
