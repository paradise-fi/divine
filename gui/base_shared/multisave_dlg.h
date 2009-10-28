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

#ifndef MULTISAVE_DLG_H_
#define MULTISAVE_DLG_H_

#include <QDialog>

namespace Ui
{
  class MultiSaveDialog;
}

class SourceEditor;

class MultiSaveDialog : public QDialog
{
    Q_OBJECT

  public:
    // button codes
    static const int btCancel = 0x1;
    static const int btSave = 0x2;
    static const int btNone = 0x4;

  public:
    MultiSaveDialog(const QList<SourceEditor*> & files, QWidget * parent = NULL);
    ~MultiSaveDialog();

    const QList<SourceEditor*> selection(void) const;

  private:
    Ui::MultiSaveDialog * ui_;

  private slots:
    void on_saveButton_clicked(void);
    void on_noneButton_clicked(void);
    void on_cancelButton_clicked(void);
};

#endif

