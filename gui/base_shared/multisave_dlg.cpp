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
#include <QListWidgetItem>
#include <QUrl>

#include "ui_multisave.h"

#include "multisave_dlg.h"
#include "editor.h"

//! Initializes the dialog with given set of files.
MultiSaveDialog::MultiSaveDialog
(const QList<SourceEditor*> & files, QWidget * parent)
{
  ui_ = new Ui::MultiSaveDialog;

  ui_->setupUi(this);

  for (QList<SourceEditor*>::const_iterator itr = files.begin();
       itr != files.end(); ++itr) {
    QUrl url((*itr)->document()->metaInformation(QTextDocument::DocumentUrl));

    QTreeWidgetItem * item = new QTreeWidgetItem(ui_->fileTree);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setText(0, (*itr)->documentTitle());
    item->setText(1, url.path());
    item->setCheckState(0, Qt::Checked);
    item->setData(0, Qt::UserRole, qVariantFromValue(static_cast<void*>(*itr)));
  }
}

MultiSaveDialog::~MultiSaveDialog()
{
  delete ui_;
}

//! Returns a list of editor the user decided to save.
const QList<SourceEditor*> MultiSaveDialog::selection(void) const
{
  QList<SourceEditor*> res;

  for (int i = 0; i < ui_->fileTree->topLevelItemCount(); ++i) {
    QTreeWidgetItem * item = ui_->fileTree->topLevelItem(i);

    if (item->checkState(0) == Qt::Checked)
      res.push_back(reinterpret_cast<SourceEditor*>(
                      item->data(0, Qt::UserRole).value<void*>()));
  }

  return res;
}

void MultiSaveDialog::on_noneButton_clicked(void)
{
  for (int i = 0; i < ui_->fileTree->topLevelItemCount(); ++i) {
    QTreeWidgetItem * item = ui_->fileTree->topLevelItem(i);
    
    item->setCheckState(0, Qt::Unchecked);
  }
  
  accept();
}
