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

#include "ui_search.h"

#include "search_dlg.h"

SearchDialog::SearchDialog(QWidget * parent)
{
  ui_ = new Ui::SearchDialog;

  ui_->setupUi(this);

  // hack to reduce flicker when adjusting size
  ui_->verticalLayout->setAlignment(Qt::AlignTop);

  connect(ui_->closeButton, SIGNAL(clicked(bool)), SLOT(close()));
  connect(ui_->nextButton, SIGNAL(clicked(bool)), SIGNAL(findNext()));
  connect(ui_->replaceButton, SIGNAL(clicked(bool)), SIGNAL(replace()));
  connect(ui_->replaceAllButton, SIGNAL(clicked(bool)), SIGNAL(replaceAll()));
}

SearchDialog::~SearchDialog()
{
  delete ui_;
}

void SearchDialog::initialize(const QString & hint, bool replace)
{
  if (replace)
    setWindowTitle(tr("Replace"));
  else
    setWindowTitle(tr("Find"));

  ui_->findEdit->setText(hint);

  ui_->replaceLabel->setVisible(replace);

  ui_->replaceEdit->setVisible(replace);

  ui_->replaceButton->setVisible(replace);

  ui_->replaceAllButton->setVisible(replace);

  ui_->replaceButton->setDefault(replace);

  ui_->nextButton->setDefault(!replace);

  updateButtonState();

  // adjust widget height
  QApplication::processEvents();

  resize(width(), 0);
}

const QRegExp SearchDialog::pattern(void) const
{
  return QRegExp(ui_->findEdit->text(), Qt::CaseSensitive,
                 ui_->regularBox->isChecked() ? QRegExp::RegExp : QRegExp::FixedString);
}

const QString SearchDialog::sample(void) const
{
  return ui_->replaceEdit->text();
}

QTextDocument::FindFlags SearchDialog::flags(void) const
{
  QTextDocument::FindFlags flags;

  bool is_regexp = ui_->regularBox->isChecked();

  if (ui_->backwardsBox->isChecked())
    flags |= QTextDocument::FindBackward;

  if (is_regexp && ui_->caseBox->isChecked())
    flags |= QTextDocument::FindCaseSensitively;

  if (is_regexp && ui_->wordsBox->isChecked())
    flags |= QTextDocument::FindWholeWords;

  return flags;
}

void SearchDialog::updateButtonState(void)
{
  bool can_find = !ui_->findEdit->text().isEmpty();
  bool can_replace = !ui_->replaceEdit->text().isEmpty();

  ui_->nextButton->setEnabled(can_find);
  ui_->replaceButton->setEnabled(can_find && can_replace);
  ui_->replaceAllButton->setEnabled(can_find && can_replace);
}

void SearchDialog::on_findEdit_textChanged(void)
{
  updateButtonState();
}

void SearchDialog::on_replaceEdit_textChanged(void)
{
  updateButtonState();
}

void SearchDialog::on_regularBox_stateChanged(int state)
{
  ui_->caseBox->setEnabled(state != Qt::Checked);
  ui_->wordsBox->setEnabled(state != Qt::Checked);
}
