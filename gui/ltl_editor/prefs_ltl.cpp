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
#include <QMenu>
#include <QAction>
#include <QSettings>
#include <QColorDialog>
#include <QHeaderView>
#include <QHBoxLayout>

#include "prefs_ltl.h"
#include "plugins.h"
#include "settings.h"

const wchar_t noneSymb[] = L"\u2014";

const LtlPreferences::HighlightInfo ltlDefs[] = {
  {false, false, false, "", ""},            // hsDefault
  {false, false, false, "darkgreen", ""},   // hsDefinition
  {false, false, false, "darkmagenta", ""}  // hsProperty
};

LtlPreferences::LtlPreferences(QWidget * parent)
  : PreferencesPage(parent)
{
  tree_ = new QTreeWidget(this);

  QHBoxLayout * hbox = new QHBoxLayout(this);
  hbox->setMargin(0);
  hbox->addWidget(tree_);

  tree_->setSelectionMode(QAbstractItemView::NoSelection);
  tree_->setContextMenuPolicy(Qt::CustomContextMenu);
  tree_->setRootIsDecorated(false);
  tree_->setSortingEnabled(false);

  QTreeWidgetItem * hdr = tree_->headerItem();
  hdr->setText(0, tr("Context"));
  hdr->setText(1, tr("Bold"));
  hdr->setText(2, tr("Italic"));
  hdr->setText(3, tr("Underline"));
  hdr->setText(4, tr("Foreground"));
  hdr->setText(5, tr("Background"));

  new QTreeWidgetItem(tree_, QStringList(tr("Default")));
  new QTreeWidgetItem(tree_, QStringList(tr("Number")));
  new QTreeWidgetItem(tree_, QStringList(tr("Definition")));
  new QTreeWidgetItem(tree_, QStringList(tr("Property")));

  //   tree_->resizeColumnToContents(0);
  tree_->resizeColumnToContents(1);
  tree_->resizeColumnToContents(2);
  tree_->resizeColumnToContents(3);
  tree_->resizeColumnToContents(4);
  tree_->resizeColumnToContents(5);

  // actions
  resetForeAct_ = new QAction(tr("Reset &foreground"), this);
  connect(resetForeAct_, SIGNAL(triggered()), SLOT(onResetForeground()));
  resetBackAct_ = new QAction(tr("Reset &background"), this);
  connect(resetBackAct_, SIGNAL(triggered()), SLOT(onResetForeground()));

  menu_ = new QMenu(this);
  menu_->addAction(resetForeAct_);
  menu_->addAction(resetBackAct_);

  connect(tree_, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)),
    SLOT(onItemDoubleClicked(QTreeWidgetItem *, int)));
  connect(tree_, SIGNAL(itemChanged(QTreeWidgetItem *, int)),
    SLOT(onItemChanged(QTreeWidgetItem *, int)));
  connect(tree_, SIGNAL(customContextMenuRequested(const QPoint &)),
    SLOT(onCustomContextMenuRequested(const QPoint &)));
}

LtlPreferences::~LtlPreferences()
{
}

void LtlPreferences::readSettings(void)
{
  QSettings & s = sSettings();
  QString str;
  QFont fnt;

  s.beginReadArray("LTL Syntax");

  for(int i = 0; i < tree_->topLevelItemCount(); ++i) {
    s.setArrayIndex(i);
    QTreeWidgetItem * item = tree_->topLevelItem(i);

    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);

    bool bold = s.value("bold", ltlDefs[i].bold).toBool();
    bool italic = s.value("italic", ltlDefs[i].italic).toBool();
    bool underline = s.value("underline", ltlDefs[i].underline).toBool();

    item->setCheckState(1, bold ? Qt::Checked : Qt::Unchecked);
    item->setCheckState(2, italic ? Qt::Checked : Qt::Unchecked);
    item->setCheckState(3, underline ? Qt::Checked : Qt::Unchecked);

    fnt = item->font(0);
    fnt.setWeight(bold ? QFont::Bold : QFont::Normal);
    fnt.setItalic(italic);
    fnt.setUnderline(underline);
    item->setFont(0, fnt);

    str = s.value("foreground", ltlDefs[i].foreground).toString();
    if(str.isEmpty()) {
      item->setText(4, QString::fromWCharArray(noneSymb));
    } else {
      item->setText(4, QColor(str).name());
      item->setForeground(4, QColor(str));
      item->setForeground(0, item->foreground(4));
    }

    str = s.value("background", ltlDefs[i].background).toString();
    if(str.isEmpty()) {
      item->setText(5, QString::fromWCharArray(noneSymb));
    } else {
      item->setText(5, QColor(str).name());
      item->setForeground(5, QColor(str));
      item->setBackground(0, item->foreground(5));
    }
  }
  s.endArray();
}

void LtlPreferences::writeSettings(void)
{
  QSettings & s = sSettings();

  s.beginReadArray("LTL Syntax");

  for(int i = 0; i < tree_->topLevelItemCount(); ++i) {
    s.setArrayIndex(i);

    QTreeWidgetItem * item = tree_->topLevelItem(i);

    s.setValue("bold", item->checkState(1) == Qt::Checked);
    s.setValue("italic", item->checkState(2) == Qt::Checked);
    s.setValue("underline", item->checkState(3) == Qt::Checked);

    if(item->text(4) == QString::fromWCharArray(noneSymb))
      s.setValue("foreground", "");
    else
      s.setValue("foreground", item->text(4));

    if(item->text(5) == QString::fromWCharArray(noneSymb))
      s.setValue("background", "");
    else
      s.setValue("background", item->text(5));
  }

  s.endArray();
}

void LtlPreferences::onItemDoubleClicked(QTreeWidgetItem * item, int column)
{
  if(column < 4 || !item)
    return;

  QString col = item->text(column);
  if(item->text(column) == QString::fromWCharArray(noneSymb)) {
    QPalette def;
    if(column == 4)
      col = def.color(QPalette::Text).name();
    else
      col = def.color(QPalette::Base).name();
  }

  QColor res = QColorDialog::getColor(QColor(col));
  if(res.isValid()) {
    item->setText(column, res.name());
    item->setForeground(column, res);
  }
  item->setForeground(0, item->foreground(4));
  item->setBackground(0, item->foreground(5));
}

void LtlPreferences::onItemChanged(QTreeWidgetItem * item, int column)
{
  if(column == 0 || column > 3 || !item)
    return;

  QFont fnt = item->font(0);
  bool checked = item->checkState(column) == Qt::Checked;

  switch(column) {
    case 1:
      fnt.setWeight(checked ? QFont::Bold : QFont::Normal);
      break;
    case 2:
      fnt.setItalic(checked);
      break;
    case 3:
      fnt.setUnderline(checked);
      break;
  }
  item->setFont(0, fnt);
}

void LtlPreferences::onCustomContextMenuRequested(const QPoint & pos)
{
  target_ = tree_->itemAt(pos);

  if(target_) {
    QPoint xpos = tree_->mapToGlobal(pos);
    QSize sz = tree_->header()->size();
    menu_->exec(xpos + QPoint(0, sz.height()));
  }
}

void LtlPreferences::onResetForeground(void)
{
  target_->setText(4, QString::fromWCharArray(noneSymb));

  QPalette def;
  target_->setForeground(4, def.color(QPalette::Text));
  target_->setForeground(0, target_->foreground(4));
}

void LtlPreferences::onResetBackground(void)
{
  Q_ASSERT(target_);

  target_->setText(5, QString::fromWCharArray(noneSymb));

  QPalette def;
  target_->setForeground(5, def.color(QPalette::Base));
  target_->setBackground(0, target_->foreground(5));
}
