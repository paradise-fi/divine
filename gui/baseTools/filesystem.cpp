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

#include <QString>
#include <QTreeView>
#include <QFileSystemModel>
#include <QHeaderView>
#include <QFileInfo>
#include <QDir>

#include "settings.h"
#include "mainForm.h"
#include "filesystem.h"

namespace divine {
namespace gui {

// TODO: finish sometimes, not a priority now
  
FileSystemBrowserDock::FileSystemBrowserDock(MainForm * root)
    : QDockWidget(tr("Filesystem"), root)
{
  model_ = new QFileSystemModel();
  
  view_ = new QTreeView(this);
  view_->setModel(model_);
  
  view_->header()->hideSection(1);
  view_->header()->hideSection(2);
  view_->header()->hideSection(3);
  view_->header()->setVisible(false);
  
  view_->setCurrentIndex(model_->index(QDir::currentPath()));

  setWidget(view_);

  readSettings();
}

//! Reloads settings.
void FileSystemBrowserDock::readSettings()
{
  QString path = Settings("common.panel.filesystem").value("path", QDir::currentPath()).toString();
  
  if(!QFileInfo(path).isDir())
    path = QDir::currentPath();

  setPath(path);
}

void FileSystemBrowserDock::writeSettings()
{
  Settings("common.panel.filesystem").setValue("path", currentPath_);
}

void FileSystemBrowserDock::setPath(const QString & path)
{
  model_->setRootPath(path);
  view_->setCurrentIndex(model_->index(path));
  
  currentPath_ = path;
}


}
}
