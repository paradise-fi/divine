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

#include <QTreeWidget>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>

#include "preferences_dlg.h"
#include "plugins.h"

PreferencesDialog::PreferencesDialog(QWidget * parent)
    : QDialog(parent)
{
  setWindowTitle(tr("Preferences"));

  QVBoxLayout * vbox = new QVBoxLayout(this);
  QHBoxLayout * hbox = new QHBoxLayout();

  groups_ = new QTreeWidget(this);

  QSizePolicy sizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
  sizePolicy.setHorizontalStretch(0);
  sizePolicy.setVerticalStretch(0);
  sizePolicy.setHeightForWidth(groups_->sizePolicy().hasHeightForWidth());

  groups_->setSizePolicy(sizePolicy);
  groups_->setMaximumSize(QSize(180, 16777215));
  groups_->setRootIsDecorated(false);
  groups_->setItemsExpandable(false);
  groups_->setHeaderHidden(true);
  groups_->setExpandsOnDoubleClick(false);

  stack_ = new QStackedWidget(this);

  hbox->addWidget(groups_);
  hbox->addWidget(stack_);
  vbox->addLayout(hbox);

  QDialogButtonBox * buttons = new QDialogButtonBox(this);
  buttons->setLayoutDirection(Qt::LeftToRight);
  buttons->setOrientation(Qt::Horizontal);
  buttons->setStandardButtons(
    QDialogButtonBox::Cancel | QDialogButtonBox::Ok);

  vbox->addWidget(buttons);

  connect(buttons, SIGNAL(accepted()), SLOT(accept()));
  connect(buttons, SIGNAL(rejected()), SLOT(reject()));

  connect(groups_,
          SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)),
          SLOT(onTabChanged(QTreeWidgetItem *, QTreeWidgetItem *)));
}

PreferencesDialog::~PreferencesDialog()
{
}

//! Adds new group to the dialog.
void PreferencesDialog::addGroup(const QString & group)
{
  for (int i = 0; i < groups_->topLevelItemCount(); ++i) {
    if (groups_->topLevelItem(i)->text(0) == group)
      return;
  }

  QTreeWidgetItem * item = new QTreeWidgetItem(groups_);

  QFont fnt = item->font(0);
  fnt.setWeight(QFont::Bold);

  item->setText(0, group);
  item->setFont(0, fnt);
  item->setExpanded(true);
  item->setFlags(item->flags() ^ Qt::ItemIsSelectable);
}

/*!
 * Registers given widget as a preferences page.
 * \param group Group of the new page.
 * \param tab Tab of the new page.
 * \param page Page widget.
 */
void PreferencesDialog::addWidget
(const QString & group, const QString & tab, QWidget * page)
{
  Q_ASSERT(page);

  addGroup(group);

  QTreeWidgetItem * item = NULL;

  for (int i = 0; i < groups_->topLevelItemCount(); ++i) {
    if (groups_->topLevelItem(i)->text(0) == group) {
      item = groups_->topLevelItem(i);
      break;
    }
  }

  Q_ASSERT(item);

  QTreeWidgetItem * sub = NULL;

  for (int i = 0; i < item->childCount(); ++i) {
    if (item->child(i)->text(0) == tab) {
      // specified tab already exists
      qDebug("Specified tab already exists: %s/%s",
             group.toAscii().data(), tab.toAscii().data());
      return;
    }
  }

  sub = new QTreeWidgetItem(item);

  sub->setText(0, tab);
  sub->setData(0, Qt::UserRole, qVariantFromValue(static_cast<void*>(page)));

  stack_->addWidget(page);

  connect(this, SIGNAL(initialized()), page, SLOT(readSettings()));
  connect(this, SIGNAL(accepted()), page, SLOT(writeSettings()));
}

//! Emits initialized signal.
void PreferencesDialog::initialize(void)
{
  emit initialized();
}

void PreferencesDialog::onTabChanged
(QTreeWidgetItem * current, QTreeWidgetItem *)
{
  QWidget * wdg = reinterpret_cast<QWidget*>(
                            current->data(0, Qt::UserRole).value<void*>());

  if (wdg)
    stack_->setCurrentWidget(wdg);
}

