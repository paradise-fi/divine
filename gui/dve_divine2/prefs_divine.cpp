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
#include <QDirModel>
#include <QCompleter>

#include "ui_prefs_divine.h"

#include "prefs_divine.h"
#include "settings.h"

// defaults
#include "dve_divine2_plugin.h"

DivinePreferences::DivinePreferences(QWidget * parent)
  : PreferencesPage(parent)
{
  ui_ = new Ui::DivinePreferences();
  ui_->setupUi(this);
  
  QCompleter * completer = new QCompleter(this);
  completer->setModel(new QDirModel(completer));
  ui_->pathEdit->setCompleter(completer);
}

DivinePreferences::~DivinePreferences()
{
  delete ui_;
}

void DivinePreferences::readSettings(void)
{
  QSettings & s  = sSettings();

  s.beginGroup("DiVinE");

  ui_->threadsSpin->setValue(s.value("threads", defDivThreads).toInt());
  ui_->memorySpin->setValue(s.value("memory", defDivMemory).toInt());
  
  ui_->pooledBox->setChecked(s.value("nopool", defDivNoPool).toBool());
  ui_->reportBox->setChecked(s.value("report", defDivReport).toBool());
  
  ui_->pathEdit->setText(s.value("path", defDivPath).toString());

  s.endGroup();
}

void DivinePreferences::writeSettings(void)
{
  QSettings & s  = sSettings();

  s.beginGroup("DiVinE");

  s.setValue("threads", ui_->threadsSpin->value());
  s.setValue("memory", ui_->memorySpin->value());
  
  s.setValue("nopool", ui_->pooledBox->isChecked());
  s.setValue("report", ui_->reportBox->isChecked());
  
  s.setValue("path", ui_->pathEdit->text());
  
  s.endGroup();
}

void DivinePreferences::on_pathButton_clicked(void)
{
  QString file = QFileDialog::getOpenFileName(this, tr("Locate divine binary"),
                                              ui_->pathEdit->text(),
#ifdef Q_OS_WIN32
                                              tr("divine.exe (divine.exe)"));
#else
                                              tr("divine (divine)"));
#endif
                                              
  
  if(!file.isEmpty())
    ui_->pathEdit->setText(file);
}
