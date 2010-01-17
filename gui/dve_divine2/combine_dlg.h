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

#ifndef COMBINE_DLG_H_
#define COMBINE_DLG_H_

#include <QDialog>

class QString;

class MainForm;

namespace Ui {
  class CombineDialog;
}

/*!
 * This class implements the combine dialog.
 */
class CombineDialog: public QDialog {
  Q_OBJECT

  public:
    CombineDialog(MainForm * root, const QString & path="");
    ~CombineDialog();

    const QString file(void) const;
    int property(void) const;
    
    const QStringList definitions(void) const;
    
  private:
    Ui::CombineDialog * ui_;
    
    QString path_;      // settings key

  private slots:
    void on_pathButton_clicked(void);
    void on_pathBox_editTextChanged(const QString & text);
    
    void onAccepted(void);
};

#endif

