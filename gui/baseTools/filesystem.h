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

#ifndef FILESYSTEM_BROWSER_H_
#define FILESYSTEM_BROWSER_H_

#include <QDockWidget>

// class QString;
// class QTextEdit;

class QTreeView;
class QFileSystemModel;

namespace divine {
namespace gui {

class MainForm;
  
// class Simulator;

// class AbstractPreferencesPage;

/*!
 * The FileSystemBrowserDock class provides interactive access to file system.
 */
class FileSystemBrowserDock : public QDockWidget {
  Q_OBJECT

  public:
    FileSystemBrowserDock(MainForm * root=NULL);

  public slots:
    void readSettings();
    void writeSettings();
    
  private:
    QTreeView * view_;
    QFileSystemModel * model_;
    
    QString currentPath_;
    
  private:
    void setPath(const QString & path);
};

}
}

#endif
