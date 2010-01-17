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

#ifndef PREFERENCES_DLG_H_
#define PREFERENCES_DLG_H_

#include <QDialog>

class QString;
class QTreeWidget;
class QTreeWidgetItem;
class QStackedWidget;
class PreferencesPage;

/*!
 * The PreferencesDialog class implements the application preferences dialog.
 */
class PreferencesDialog: public QDialog
{
    Q_OBJECT

  public:
    PreferencesDialog(QWidget * parent = NULL);
    ~PreferencesDialog();

    void addGroup(const QString & group);
    void addWidget(const QString & group, const QString & tab, QWidget * page);

    void initialize(void);

  signals:
    //! Sends initialization request to registered pages.
    void initialized();

  private:
    QTreeWidget * groups_;
    QStackedWidget * stack_;

  private slots:
    void onTabChanged(QTreeWidgetItem * current, QTreeWidgetItem * previous);
};

#endif
