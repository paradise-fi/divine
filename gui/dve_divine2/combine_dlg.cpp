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

#include <QFileDialog>
#include <QCompleter>
#include <QDirModel>
#include <QTextStream>
#include <QPushButton>

#include "ui_combine.h"

#include "settings.h"
#include "mainform.h"
#include "combine_dlg.h"

/*!
 * Initializes the combine dialog.
 * \param root Application's main window.
 * \param path Path to the initial LTL file.
 */
CombineDialog::CombineDialog(MainForm * root, const QString & path)
  : QDialog(root), path_(path)
{
  ui_ = new Ui::CombineDialog;

  ui_->setupUi(this);

  foreach(QString str, root->openedFiles()) {
    if(QFileInfo(str).suffix() == "ltl" || QFileInfo(str).suffix() == "mltl")
      ui_->pathBox->addItem(str);
  }
  
  sSettings().beginGroup("DiVinE");
  
  QString last = sSettings().value("combineLast", "").toString();
  QFileInfo finfo(last);
  
  if (finfo.exists() && finfo.suffix() == "ltl")
    ui_->pathBox->setEditText(last);
  
  sSettings().endGroup();
  
  QCompleter * completer = new QCompleter(this);
  completer->setModel(new QDirModel(completer));

  sSettings().beginGroup("Combine");
  ui_->defsEdit->setText(sSettings().value(path, "").toString());
  sSettings().endGroup();
  
  ui_->pathBox->setCompleter(completer);
  
  connect(this, SIGNAL(accepted()), SLOT(onAccepted()));
}

CombineDialog::~CombineDialog()
{
  delete ui_;
}

//! Gets the selected LTL file
const QString CombineDialog::file(void) const
{
  QFileInfo info(ui_->pathBox->currentText());
  
  if(!info.isFile() || !info.isReadable() || info.suffix() != "ltl")
    return QString();
  
  return info.absoluteFilePath();
}

//! Gets the number of the property selected by user.
int CombineDialog::property(void) const
{
  return ui_->formulaList->currentRow();
}

//! Gets a list of definitions specified by user.
const QStringList CombineDialog::definitions(void) const
{
  if(ui_->defsEdit->text().simplified().isEmpty())
    return QStringList();
  
  return ui_->defsEdit->text().simplified().split(" ");
}

void CombineDialog::on_pathButton_clicked(void)
{
  QString file = QFileDialog::getOpenFileName(this, tr("Select LTL formula"),
                                              ui_->pathBox->currentText(),
                                              tr("LTL Formulas (*.ltl *.mltl)"));
  
  if(!file.isEmpty())
    ui_->pathBox->setEditText(file);
}

void CombineDialog::on_pathBox_editTextChanged(const QString & text)
{
  ui_->formulaList->clear();
  
  if(text.isEmpty()) {
    ui_->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
    return;
  }
 
  QFileInfo info(text);
 
  if(!info.isFile() || !info.isReadable() || (info.suffix() != "ltl" && info.suffix() != "mltl")) {
    ui_->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    return;
  }

  // reload the LTL formula
  QFile file(text);

  if(!file.open(QFile::ReadOnly | QFile::Text))
    return;

  QRegExp propRe("^\\s*#property\\s+(\\S.*)$");
  
  QTextStream in(&file);
  QString line;
  
  do {
     line = in.readLine();
     
     if(line.contains(propRe))
       ui_->formulaList->addItem(propRe.capturedTexts().at(1).simplified());
  } while(!line.isNull());
  
  ui_->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(ui_->formulaList->count() > 0);
  
  if(ui_->formulaList->currentRow() == -1)
    ui_->formulaList->setCurrentRow(0);
}

void CombineDialog::onAccepted(void)
{
  sSettings().beginGroup("DiVinE");
  sSettings().setValue("combineLast", ui_->pathBox->currentText());
  sSettings().endGroup();
  
  sSettings().beginGroup("Combine");
  sSettings().setValue(path_, ui_->defsEdit->text());
  sSettings().endGroup();
}