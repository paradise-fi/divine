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

#ifndef LAYOUT_H_
#define LAYOUT_H_

#include "base_shared_export.h"

#include <QHash>
#include <QObject>
#include <QPair>

class QWidget;
class QAction;
class QMainWindow;

/*!
 * The LayoutSet class stores the visibility of widgets and action in one layout.
 * \see LayoutManager
 */
class LayoutSet
{
  public:
    void makeVisible(void);

    void addWidget(QWidget * widget, bool state);
    void removeWidget(QWidget * widget);

    void addAction(QAction * action, bool state);
    void removeAction(QAction * action);

  private:
    typedef QHash<QWidget*, bool> WidgetHash;
    typedef QHash<QAction*, bool> ActionHash;

  private:
    WidgetHash widgets_;
    ActionHash actions_;
};

/*!
 * The LayoutManager class provides mechanism for managing application layouts.
 * It helps serialize geometry and visibility of widgets and actions among
 * number of layouts.
 */
class BASE_SHARED_EXPORT LayoutManager : public QObject
{
    Q_OBJECT

  public:
    LayoutManager(QMainWindow * parent, const QStringList & layouts);
    ~LayoutManager();

    void switchLayout(const QString & layout);

    void readSettings(void);
    void writeSettings(void);

    void addWidget(QWidget * widget, const QStringList & layouts);
    void removeWidget(QWidget * widget);

    void addAction(QAction * action, const QStringList & layouts);
    void removeAction(QAction * action);

  private:
    typedef QPair<LayoutSet, QByteArray> LayoutPair;
    typedef QHash<QString, LayoutPair> LayoutHash;

  private:
    QMainWindow * parent_;

    QString active_;
    LayoutHash layouts_;
};

#endif
