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

#ifndef RECENT_H_
#define RECENT_H_

#include <QMenu>
#include <QStringList>

class RecentFilesMenu : public QMenu
{
    Q_OBJECT

  public:
    RecentFilesMenu(const QString & title, int max, QWidget * parent = NULL);

    void addFile(const QString & fileName);

  signals:
    void triggered(const QString & fileName);

  private:
    int max_;

  private:
    void updateActions(void);
};

#endif
