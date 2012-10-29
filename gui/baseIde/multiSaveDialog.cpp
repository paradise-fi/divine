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

#include <QTreeWidgetItem>

#include "ui_multiSaveDialog.h"

#include "multiSaveDialog.h"
#include "abstractEditor.h"

namespace divine {
namespace gui {

//! Initializes the dialog with given set of files.
MultiSaveDialog::MultiSaveDialog
(const QList<AbstractEditor*> & files, QWidget * parent)
{
  ui_ = new Ui::MultiSaveDialog;

  ui_->setupUi(this);
  
  foreach (AbstractEditor * editor, files) {
    // documents are responsible for announcing modified state in a sensible way
    // ie. not twice on instances of the same file
    QTreeWidgetItem * item = new QTreeWidgetItem(ui_->fileTree);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setText(0, editor->viewName());
    item->setText(1, editor->sourcePath());
    item->setCheckState(0, Qt::Checked);
    item->setData(0, Qt::UserRole, qVariantFromValue(static_cast<void*>(editor)));
  }
}

MultiSaveDialog::~MultiSaveDialog()
{
  delete ui_;
}

//! Returns a list of editor the user decided to save.
const QList<AbstractEditor*> MultiSaveDialog::selection() const
{
  QList<AbstractEditor*> res;

  for (int i = 0; i < ui_->fileTree->topLevelItemCount(); ++i) {
    QTreeWidgetItem * item = ui_->fileTree->topLevelItem(i);

    if (item->checkState(0) == Qt::Checked)
      res.push_back(reinterpret_cast<AbstractEditor*>(
                      item->data(0, Qt::UserRole).value<void*>()));
  }

  return res;
}

void MultiSaveDialog::on_noneButton_clicked()
{
  // clear selection
  ui_->fileTree->clear();
  
  accept();
}

}
}
