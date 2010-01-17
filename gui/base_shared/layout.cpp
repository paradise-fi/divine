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

#include <QWidget>
#include <QAction>
#include <QMainWindow>
#include <QSettings>

#include "settings.h"
#include "layout.h"

//! Sets visibility for registered widgets and actions.
void LayoutSet::makeVisible(void)
{
  for (WidgetHash::iterator itr = widgets_.begin();
       itr != widgets_.end(); ++itr) {
    itr.key()->setVisible(itr.value());
  }

  for (ActionHash::iterator itr = actions_.begin();
       itr != actions_.end(); ++itr) {
    itr.key()->setVisible(itr.value());
  }
}

/*!
 * Adds widget to the layout.
 * \param widget Widget.
 * \param state Widget's visibility in this layout.
 */
void LayoutSet::addWidget(QWidget * widget, bool state)
{
  widgets_[widget] = state;
}

//! Unregisters widget from this layout.
void LayoutSet::removeWidget(QWidget * widget)
{
  widgets_.remove(widget);
}

/*!
 * Adds widget to the layout.
 * \param action Action.
 * \param state Action's visibility in this layout.
 */
void LayoutSet::addAction(QAction * action, bool state)
{
  actions_[action] = state;
}

//! Unregisters action from this layout.
void LayoutSet::removeAction(QAction * action)
{
  actions_.remove(action);
}

/*!
 * Constructor of the LayoutManager class. The new instance is initialized
 * with given set of layouts.
 * \param parent Parent object.
 * \param layouts List of layout names.
 */
LayoutManager::LayoutManager(QMainWindow* parent, const QStringList& layouts)
    : QObject(parent), parent_(parent), active_(-1)
{
  Q_ASSERT(parent);

  foreach(QString str, layouts) {
    layouts_.insert(str, LayoutPair());
  }
}

LayoutManager::~LayoutManager()
{
}

//! Switches to requested layout.
void LayoutManager::switchLayout(const QString & layout)
{
  Q_ASSERT(layouts_.contains(layout));

  if (layout == active_)
    return;

  // save current layout
  if (!active_.isEmpty())
    layouts_[active_].second = parent_->saveState();

  // switch layouts
  active_ = layout;

  layouts_[active_].first.makeVisible();

  // and restore state
  parent_->restoreState(layouts_[active_].second);
}

//! Loads layout configuration (geometry) from application settings.
void LayoutManager::readSettings(void)
{
  QSettings & s = sSettings();

  s.beginGroup("Layouts");

  for (LayoutHash::iterator itr = layouts_.begin();
       itr != layouts_.end(); ++itr) {
    itr.value().second = s.value(itr.key(), QByteArray()).toByteArray();
  }

  s.endGroup();
}

//! Stores layout configuration to application settings.
void LayoutManager::writeSettings(void)
{
  // update current layout state
  if (!active_.isEmpty())
    layouts_[active_].second = parent_->saveState();

  QSettings & s = sSettings();

  s.beginGroup("Layouts");

  for (LayoutHash::iterator itr = layouts_.begin();
       itr != layouts_.end(); ++itr) {
    s.setValue(itr.key(), itr.value().second);
  }

  s.endGroup();
}

/*!
 * Registers widget.
 * \param widget Widget.
 * \param states List of layouts in which the widget is visible.
 */
void LayoutManager::addWidget(QWidget* widget, const QStringList& states)
{
  for (LayoutHash::iterator itr = layouts_.begin();
       itr != layouts_.end(); ++itr) {
    itr.value().first.addWidget(widget, states.contains(itr.key()));
  }

  widget->setVisible(states.contains(active_));
}

//! Unregisters widget.
void LayoutManager::removeWidget(QWidget * widget)
{
  foreach(LayoutHash::mapped_type itr, layouts_) {
    itr.first.removeWidget(widget);
  }
}

/*!
 * Registers action.
 * \param action Action.
 * \param states List of layouts in which the action is visible.
 */
void LayoutManager::addAction(QAction* action, const QStringList& states)
{
  for (LayoutHash::iterator itr = layouts_.begin();
       itr != layouts_.end(); ++itr) {
    itr.value().first.addAction(action, states.contains(itr.key()));
  }

  action->setVisible(states.contains(active_));
}

//! Unregisters action.
void LayoutManager::removeAction(QAction * action)
{
  foreach(LayoutHash::mapped_type itr, layouts_) {
    itr.first.removeAction(action);
  }
}
