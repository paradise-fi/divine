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

#ifndef NEW_DOCUMENT_DIALOG_H_
#define NEW_DOCUMENT_DIALOG_H_

#include <QDialog>

#include "moduleManager.h"

namespace Ui
{
  class NewDocumentDialog;
}

namespace divine {
namespace gui {

class AbstractEditor;

/*!
 * The MultiSaveDialog class implements the dialog window asking the user
 * to save multiple files.
 */
class NewDocumentDialog : public QDialog
{
    Q_OBJECT

  public:
    NewDocumentDialog(const ModuleManager::DocumentList & docs, QWidget * parent = NULL);
    ~NewDocumentDialog();

    QString selection() const;

  private:
    Ui::NewDocumentDialog * ui_;
    
  private slots:  
    void on_documentList_itemSelectionChanged();
};

}
}

#endif
