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

#include <QAction>
#include <QFileInfo>

#include "recent.h"
#include "settings.h"

/*!
 * initializes the Recent Files menu.
 * \param title Menu title.
 * \param max Max. number of stored files.
 * \param parent Parent widget.
 */
RecentFilesMenu::RecentFilesMenu(const QString & title, int max, QWidget * parent)
    : QMenu(title, parent), max_(max)
{
  QAction * action;

  // init actions
  for (int i = 0; i < max_; ++i) {
    action = new QAction(this);
    addAction(action);
  }

  updateActions();
}

//! Adds file to the history discarding the oldest one if the limit is reached.
void RecentFilesMenu::addFile(const QString & fileName)
{
  if (fileName.isEmpty())
    return;

  QStringList files = sSettings().value("recentFiles", QStringList()).toStringList();

  QString fullName = QFileInfo(fileName).absoluteFilePath();

  files.removeAll(fullName);
  files.prepend(fileName);

  if (files.size() > max_)
    files.removeLast();

  sSettings().setValue("recentFiles", files);

  updateActions();
}

void RecentFilesMenu::updateActions(void)
{
  QStringList files = sSettings().value("recentFiles", QStringList()).toStringList();
  QAction * action;

  for (int i = 0; i < max_; ++i) {
    action = actions().at(i);

    if (i < files.size()) {
      action->setText(QFileInfo(files[i]).fileName());
      action->setData(files[i]);
      action->setVisible(true);
    } else {
      action->setVisible(false);
    }
  }

  setEnabled(!files.isEmpty());
}
