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

#ifndef COMBINE_DIALOG_H_
#define COMBINE_DIALOG_H_

#include <QDialog>
#include <QStringList>

class QString;
class QListWidgetItem;

namespace Ui {
  class CombineDialog;
}

namespace divine {
namespace gui {

class MainForm;
class AbstractDocument;

/*!
 * This class implements the combine dialog.
 */
class CombineDialog: public QDialog {
  Q_OBJECT

  public:
    CombineDialog(MainForm * root, AbstractDocument * doc);
    ~CombineDialog();

    QString fileName() const;
    QString property() const;
    int propertyID() const;

  private:
    Ui::CombineDialog * ui_;
    AbstractDocument * doc_;    // settings key

  private slots:
    void updateButtons();

    void on_importButton_clicked();
    void on_manualButton_clicked();
    void on_formulaList_itemActivated(QListWidgetItem * item);

    void onAccepted();

  private:
    void importLtlFile(const QString & fileName, bool quiet);
};

}
}

#endif
