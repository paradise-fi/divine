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

#include <QLineEdit>
#include <QPushButton>

#include "searchPanel.h"
#include "mainForm.h"

#include "ui_searchPanel.h"

namespace divine {
namespace gui {
  
SearchPanel::SearchPanel(QWidget * parent)
{
  ui_ = new Ui::SearchPanel;

  ui_->setupUi(this);
  
  ui_->hideButton->setIcon(MainForm::loadIcon(":/icons/common/close"));
  ui_->nextButton->setIcon(MainForm::loadIcon(":/icons/edit/find-next"));
  ui_->prevButton->setIcon(MainForm::loadIcon(":/icons/edit/find-prev"));

  connect(ui_->findEdit, SIGNAL(returnPressed()), SIGNAL(findNext()));
  connect(ui_->replaceEdit, SIGNAL(returnPressed()), SIGNAL(replace()));
  
  connect(ui_->nextButton, SIGNAL(clicked(bool)), SIGNAL(findNext()));
  connect(ui_->prevButton, SIGNAL(clicked(bool)), SIGNAL(findPrevious()));
  connect(ui_->replaceButton, SIGNAL(clicked(bool)), SIGNAL(replace()));
  connect(ui_->replaceAllButton, SIGNAL(clicked(bool)), SIGNAL(replaceAll()));
}

SearchPanel::~SearchPanel()
{
  delete ui_;
}

void SearchPanel::show(const QString & hint, bool replace)
{
  if(!hint.isEmpty()) {
    // suppress the findNext signal
    ui_->findEdit->blockSignals(true);
    ui_->findEdit->setText(hint);
    ui_->findEdit->blockSignals(false);
  }
  
  ui_->replaceLabel->setVisible(replace);
  ui_->replaceEdit->setVisible(replace);
  ui_->replaceButton->setVisible(replace);
  ui_->replaceAllButton->setVisible(replace);

  updateButtonState();
  
  QWidget::show();
  
  ui_->findEdit->selectAll();
  ui_->findEdit->setFocus();
}

void SearchPanel::hideEvent(QHideEvent * event)
{
  emit closed();
}

const bool SearchPanel::modeRegEx() const
{
  return ui_->regularBox->isChecked();
}

const bool SearchPanel::modeCaseSensitive() const
{
  return ui_->caseBox->isChecked();
}

const bool SearchPanel::modeWholeWords() const
{
  return !modeRegEx() && ui_->wordsBox->isChecked();
}

//! Returns the query as a regular expression pattern.
const QRegExp SearchPanel::pattern() const
{
  return QRegExp(ui_->findEdit->text(),
                 modeCaseSensitive() ? Qt::CaseSensitive : Qt::CaseInsensitive,
                 modeRegEx() ? QRegExp::RegExp : QRegExp::FixedString);
}

//! Returns the replacement sample.
const QString SearchPanel::sample() const
{
  return ui_->replaceEdit->text();
}

void SearchPanel::updateButtonState()
{
  bool can_find = !ui_->findEdit->text().isEmpty();

  ui_->nextButton->setEnabled(can_find);
  ui_->prevButton->setEnabled(can_find);
  ui_->replaceButton->setEnabled(can_find);
  ui_->replaceAllButton->setEnabled(can_find);
  
  emit searchAvailable(can_find);
}

void SearchPanel::on_findEdit_textChanged()
{
  updateButtonState();
  
  if(!ui_->findEdit->text().isEmpty())
    emit findNext();
}

void SearchPanel::on_regularBox_stateChanged(int state)
{
  ui_->wordsBox->setEnabled(state != Qt::Checked);
}

}
}
