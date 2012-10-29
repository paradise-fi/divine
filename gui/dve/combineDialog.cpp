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

#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>

#include "ui_combineDialog.h"

#include "settings.h"
#include "mainForm.h"
#include "abstractDocument.h"
#include "combineDialog.h"

namespace divine {
namespace gui {

CombineDialog::CombineDialog(MainForm * root, AbstractDocument * doc)
  : QDialog(root), doc_(doc)
{
  ui_ = new Ui::CombineDialog;

  ui_->setupUi(this);

  ui_->importButton->setIcon(MainForm::loadIcon(":/icons/file/open"));
  ui_->manualButton->setIcon(MainForm::loadIcon(":/icons/common/close"));

  Settings s(doc_);
  s.beginGroup("dve/combine");
  QString property = s.value("property").toString();
  QString import = s.value("import").toString();
  s.endGroup();

  // setup property
  if(!import.isEmpty()) {
    importLtlFile(import, true);
  } else {
    ui_->propertyEdit->setText(property);
  }
  updateButtons();

  connect(ui_->propertyEdit, SIGNAL(textChanged(QString)), SLOT(updateButtons()));
  connect(this, SIGNAL(accepted()), SLOT(onAccepted()));
}

CombineDialog::~CombineDialog()
{
  delete ui_;
}

QString CombineDialog::fileName() const
{
  return ui_->propertyEdit->isEnabled() ? "" : ui_->propertyEdit->text();
}

QString CombineDialog::property() const
{
  if(ui_->propertyEdit->isEnabled()) {
    return ui_->propertyEdit->text();
  } else {
    return ui_->formulaList->currentItem()->text();
  }
}

int CombineDialog::propertyID() const
{
  return ui_->propertyEdit->isEnabled() ? 0 : ui_->formulaList->currentIndex().row();
}

void CombineDialog::updateButtons()
{
  ui_->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(
    !ui_->propertyEdit->isEnabled() || !ui_->propertyEdit->text().isEmpty());
}

void CombineDialog::on_importButton_clicked()
{
  QString fileName = QFileDialog::getOpenFileName(
    this, tr("Select LTL formula"), QFileInfo(doc_->path()).absolutePath(),
    tr("LTL Formulas (*.ltl) (*.ltl)"));

  if(fileName.isEmpty())
    return;

  importLtlFile(fileName, false);
  updateButtons();
}

void CombineDialog::on_manualButton_clicked()
{
  ui_->manualButton->setEnabled(false);

  ui_->formulaList->clear();
  ui_->formulaList->setEnabled(false);

  ui_->propertyEdit->setEnabled(true);
  ui_->propertyEdit->setText("");

  updateButtons();
}

void CombineDialog::on_formulaList_itemActivated(QListWidgetItem * item)
{
  // just to be sure
  if(item->text().isEmpty())
    return;

  accept();
}

void CombineDialog::onAccepted()
{
  Settings s(doc_);
  s.beginGroup("dve/combine");
  s.setValue("property", property());
  s.setValue("import", fileName());
  s.endGroup();
}

void CombineDialog::importLtlFile(const QString & fileName, bool quiet)
{
  QFile file(fileName);

  if(!file.open(QFile::ReadOnly | QFile::Text)) {
    if(!quiet)
      QMessageBox::warning(this, tr("DiVinE IDE"),
                          tr("Cannot open file '%1'.").arg(fileName));
    return;
  }

  QRegExp propRe("^\\s*#property\\s+(\\S.*)$");

  QTextStream in(&file);
  QStringList props;
  QString line;

  do {
    line = in.readLine();

    if(line.contains(propRe))
      props.append(propRe.capturedTexts().at(1).simplified());
  } while(!line.isNull());

  if(props.isEmpty()) {
    if(!quiet)
      QMessageBox::warning(this, tr("DiVinE IDE"),
                          tr("No properties found in file '%1'.").arg(fileName));
    return;
  }

  ui_->formulaList->clear();

  foreach(QString prop, props) {
    ui_->formulaList->addItem(prop);
  }
  ui_->formulaList->setCurrentRow(0);

  ui_->propertyEdit->setEnabled(false);
  ui_->propertyEdit->setText(fileName);

  ui_->manualButton->setEnabled(true);
  ui_->formulaList->setEnabled(true);
}

}
}
