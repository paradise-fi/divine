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

#ifndef PREFS_LTL_H_
#define PREFS_LTL_H_

#include "plugins.h"

class QTreeWidget;
class QTreeWidgetItem;
class QMenu;
class QAction;

struct HighlightInfo {
  bool bold;
  bool italic;
  bool underline;
  const char * foreground;
  const char * background;
};

extern const HighlightInfo ltlDefs[];

class LtlPreferences : public PreferencesPage {
  Q_OBJECT

  public:
    LtlPreferences(QWidget * parent=NULL);
    ~LtlPreferences();

  public slots:
    void readSettings(void);
    void writeSettings(void);

  private:
    QTreeWidget * tree_;

    QTreeWidgetItem * target_;

    QMenu * menu_;
    QAction * resetForeAct_;
    QAction * resetBackAct_;

  private slots:
    void onItemDoubleClicked(QTreeWidgetItem * item, int column);
    void onItemChanged(QTreeWidgetItem * item, int column);
    void onCustomContextMenuRequested(const QPoint & pos);
    void onResetForeground(void);
    void onResetBackground(void);
};

#endif
