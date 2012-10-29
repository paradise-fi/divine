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

#include <QListWidgetItem>

#include "ui_newDocumentDialog.h"

#include "plugins.h"

#include "newDocumentDialog.h"
#include "mainForm.h"

namespace divine {
namespace gui {

//! Initializes the dialog with given set of document types.
NewDocumentDialog::NewDocumentDialog
(const PluginManager::DocumentList & docs, QWidget * parent)
{
  ui_ = new Ui::NewDocumentDialog;

  ui_->setupUi(this);

  QSet<QString> used;
  
  foreach (PluginManager::DocumentList::value_type itr, docs) {
    QListWidgetItem * item = new QListWidgetItem(ui_->documentList);
    item->setText(itr->name());
    item->setIcon(itr->icon());
    item->setData(Qt::ToolTipRole, itr->description());
    item->setData(Qt::UserRole, itr->suffix());             // return value
  }
  
  ui_->okButton->setEnabled(false);
  
  connect(ui_->documentList, SIGNAL(itemActivated(QListWidgetItem*)), ui_->okButton, SLOT(click()));
}

NewDocumentDialog::~NewDocumentDialog()
{
  delete ui_;
}

//! Returns the suffix of the selected document type.
QString NewDocumentDialog::selection() const
{
  QList<QListWidgetItem*> selection = ui_->documentList->selectedItems();
  
  if (selection.isEmpty())
    return QString();
  
  return selection.front()->data(Qt::UserRole).toString();
}

void NewDocumentDialog::on_documentList_itemSelectionChanged()
{
  ui_->okButton->setEnabled(!ui_->documentList->selectedItems().isEmpty());
}

}
}
