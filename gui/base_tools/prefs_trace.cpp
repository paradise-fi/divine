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

#include <QColorDialog>

#include "ui_prefs_trace.h"

#include "prefs_trace.h"
#include "settings.h"

// defaults
#include "base_tools.h"

TracePreferences::TracePreferences(QWidget * parent)
  : PreferencesPage(parent)
{
  ui_ = new Ui::TracePreferences();
  ui_->setupUi(this);
}

TracePreferences::~TracePreferences()
{
  delete ui_;
}

void TracePreferences::readSettings(void)
{
  QSettings & s  = sSettings();

  s.beginGroup("Stack");

  ui_->variableBox->setChecked(
    s.value("variable", defTraceVars).toBool());
  ui_->variableNameBox->setChecked(
    s.value("variableName", defTraceVarNames).toBool());
  ui_->processBox->setChecked(s.value("process", defTraceProcs).toBool());
  ui_->processNameBox->setChecked(s.value(
    "processName", defTraceProcNames).toBool());
  ui_->channelBox->setChecked(s.value("channel", defTraceBufs).toBool());
  ui_->channelNameBox->setChecked(s.value("channelName", defTraceBufNames).toBool());

  ui_->deadButton->setCurrentColor(
    s.value("deadlock", defTraceDeadlock).value<QColor>());
  ui_->errorButton->setCurrentColor(
    s.value("error", defTraceError).value<QColor>());

  s.endGroup();
}

void TracePreferences::writeSettings(void)
{
  QSettings & s  = sSettings();

  s.beginGroup("Stack");

  s.setValue("variable", ui_->variableBox->isChecked());
  s.setValue("variableName", ui_->variableNameBox->isChecked());
  s.setValue("process", ui_->processBox->isChecked());
  s.setValue("processName", ui_->processNameBox->isChecked());
  s.setValue("channel", ui_->channelBox->isChecked());
  s.setValue("channelName", ui_->channelNameBox->isChecked());

  s.setValue("deadlock", ui_->deadButton->currentColor());
  s.setValue("error", ui_->errorButton->currentColor());

  s.endGroup();
}

void TracePreferences::on_variableBox_stateChanged(int state)
{
  ui_->variableNameBox->setEnabled(state == Qt::Checked);
}

void TracePreferences::on_processBox_stateChanged(int state)
{
  ui_->processNameBox->setEnabled(state == Qt::Checked);
}

void TracePreferences::on_channelBox_stateChanged(int state)
{
  ui_->channelNameBox->setEnabled(state == Qt::Checked);
}
