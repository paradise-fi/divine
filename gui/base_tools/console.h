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

#ifndef CONSOLE_H_
#define CONSOLE_H_

#include <QDockWidget>

class QString;
class QTextEdit;

class Simulator;

class PreferencesPage;

class ConsoleDock : public QDockWidget {
  Q_OBJECT

  public:
    static PreferencesPage * createPreferencesPage(QWidget * parent);

  public:
    ConsoleDock(QWidget * parent=NULL);

    bool isEmpty(void) const;

  public slots:
    void readSettings(void);
    void clear(void);
    void appendText(const QString & text);

  private:
    QTextEdit * edit_;
};

#endif
