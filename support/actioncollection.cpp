/***************************************************************************
 *   Copyright (C) 2005-09 by the Quassel Project                          *
 *   devel@quassel-irc.org                                                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************
 * Parts of this implementation are based on KDE's KActionCollection.      *
 ***************************************************************************/

#include "actioncollection.h"
#include "action.h"
#include "icon.h"
#include <QCoreApplication>

static const char *constProp="Category";
static ActionCollection *coll=0;
static QWidget *mainWidget=0;
void ActionCollection::setMainWidget(QWidget *w)
{
    mainWidget=w;
}

ActionCollection * ActionCollection::get()
{
    if (!coll) {
        coll=new ActionCollection(mainWidget);
        coll->setProperty(constProp, QCoreApplication::applicationName());
        if (mainWidget) {
            coll->addAssociatedWidget(mainWidget);
        }
    }
    return coll;
}

Action * ActionCollection::createAction(const QString &name, const QString &text, const char *icon, const QString &whatsThis)
{
    Action *act = static_cast<Action *>(addAction(name));
    act->setText(text);
    if (0!=icon) {
        #ifndef ENABLE_KDE_SUPPORT
        if ('m'==icon[0] && 'e'==icon[1] && 'd'==icon[2] && 'i'==icon[3] && 'a'==icon[4] && '-'==icon[5]) {
            act->setIcon(Icon::getMediaIcon(icon));
        } else
        #endif
        act->setIcon(Icon(icon));
    }
    if (!whatsThis.isEmpty()) {
        act->setWhatsThis(whatsThis);
    }
    return act;
}

Action * ActionCollection::createAction(const QString &name, const QString &text, const Icon &icon, const QString &whatsThis)
{
    Action *act = static_cast<Action *>(addAction(name));
    act->setText(text);
    if (!icon.isNull()) {
        act->setIcon(icon);
    }
    if (!whatsThis.isEmpty()) {
        act->setWhatsThis(whatsThis);
    }
    return act;
}

#ifndef ENABLE_KDE_SUPPORT

#include <QSettings>

ActionCollection::ActionCollection(QObject *parent) : QObject(parent) {
  _connectTriggered = _connectHovered = false;
}

ActionCollection::~ActionCollection() {
}

void ActionCollection::clear() {
  _actionByName.clear();
  qDeleteAll(_actions);
  _actions.clear();
}

QAction *ActionCollection::action(const QString &name) const {
  return _actionByName.value(name, 0);
}

QList<QAction *> ActionCollection::actions() const {
  return _actions;
}

Action *ActionCollection::addAction(const QString &name, Action *action) {
  QAction *act = addAction(name, static_cast<QAction *>(action));
  Q_UNUSED(act);
  Q_ASSERT(act == action);
  return action;
}

Action *ActionCollection::addAction(const QString &name, const QObject *receiver, const char *member) {
  Action *a = new Action(this);
  if(receiver && member)
    connect(a, SIGNAL(triggered(bool)), receiver, member);
  return addAction(name, a);
}

QAction *ActionCollection::addAction(const QString &name, QAction *action) {
  if(!action)
    return action;

  const QString origName = action->objectName();
  QString indexName = name;

  if(indexName.isEmpty())
    indexName = action->objectName();
  else
    action->setObjectName(indexName);
  if(indexName.isEmpty())
    indexName = indexName.sprintf("unnamed-%p", (void *)action);

  // do we already have this action?
  if(_actionByName.value(indexName, 0) == action)
    return action;
  // or maybe another action under this name?
  if(QAction *oldAction = _actionByName.value(indexName))
    takeAction(oldAction);

  // do we already have this action under a different name?
  int oldIndex = _actions.indexOf(action);
  if(oldIndex != -1) {
    _actionByName.remove(origName);
    _actions.removeAt(oldIndex);
  }

  // add action
  _actionByName.insert(indexName, action);
  _actions.append(action);

  foreach(QWidget *widget, _associatedWidgets) {
    widget->addAction(action);
  }

  connect(action, SIGNAL(destroyed(QObject *)), SLOT(actionDestroyed(QObject *)));
  if(_connectHovered)
    connect(action, SIGNAL(hovered()), SLOT(slotActionHovered()));
  if(_connectTriggered)
    connect(action, SIGNAL(triggered(bool)), SLOT(slotActionTriggered()));

  emit inserted(action);
  return action;
}

void ActionCollection::removeAction(QAction *action) {
  delete takeAction(action);
}

QAction *ActionCollection::takeAction(QAction *action) {
  if(!unlistAction(action))
    return 0;

  foreach(QWidget *widget, _associatedWidgets) {
    widget->removeAction(action);
  }

  action->disconnect(this);
  return action;
}

QString ActionCollection::configKey() const
{
    QVariant v=property(constProp);
    if (v.isValid() && !v.toString().isEmpty()) {
        return QLatin1String("Shortcuts-")+v.toString();
    }
    return QLatin1String("Shortcuts");
}

void ActionCollection::readSettings() {
  QSettings s;
  s.beginGroup(configKey());
  QStringList savedShortcuts = s.childKeys();

  foreach(const QString &name, _actionByName.keys()) {
    if(!savedShortcuts.contains(name))
      continue;
    Action *action = qobject_cast<Action *>(_actionByName.value(name));
    if(action)
      action->setShortcut(s.value(name, QKeySequence()).value<QKeySequence>(), Action::ActiveShortcut);
  }
}

void ActionCollection::writeSettings() const {
  QSettings s;
  s.beginGroup(configKey());
  foreach(const QString &name, _actionByName.keys()) {
    Action *action = qobject_cast<Action *>(_actionByName.value(name));
    if(!action)
      continue;
    if(!action->isShortcutConfigurable())
      continue;
    if(action->shortcut(Action::ActiveShortcut) == action->shortcut(Action::DefaultShortcut))
      continue;
    s.setValue(name, action->shortcut(Action::ActiveShortcut));
  }
}

void ActionCollection::slotActionTriggered() {
  QAction *action = qobject_cast<QAction *>(sender());
  if(action)
    emit actionTriggered(action);
}

void ActionCollection::slotActionHovered() {
  QAction *action = qobject_cast<QAction *>(sender());
  if(action)
    emit actionHovered(action);
}

void ActionCollection::actionDestroyed(QObject *obj) {
  // remember that this is not an QAction anymore at this point
  QAction *action = static_cast<QAction *>(obj);

  unlistAction(action);
}

void ActionCollection::connectNotify(const char *signal) {
  if(_connectHovered && _connectTriggered)
    return;

  if(QMetaObject::normalizedSignature(SIGNAL(actionHovered(QAction*))) == signal) {
    if(!_connectHovered) {
      _connectHovered = true;
      foreach (QAction* action, actions())
        connect(action, SIGNAL(hovered()), SLOT(slotActionHovered()));
    }
  } else if(QMetaObject::normalizedSignature(SIGNAL(actionTriggered(QAction*))) == signal) {
    if(!_connectTriggered) {
      _connectTriggered = true;
      foreach (QAction* action, actions())
        connect(action, SIGNAL(triggered(bool)), SLOT(slotActionTriggered()));
    }
  }

  QObject::connectNotify(signal);
}

void ActionCollection::associateWidget(QWidget *widget) const {
  foreach(QAction *action, actions()) {
    if(!widget->actions().contains(action))
      widget->addAction(action);
  }
}

void ActionCollection::addAssociatedWidget(QWidget *widget) {
  if(!_associatedWidgets.contains(widget)) {
    widget->addActions(actions());
    _associatedWidgets.append(widget);
    connect(widget, SIGNAL(destroyed(QObject *)), SLOT(associatedWidgetDestroyed(QObject *)));
  }
}

void ActionCollection::removeAssociatedWidget(QWidget *widget) {
  foreach(QAction *action, actions())
    widget->removeAction(action);
  _associatedWidgets.removeAll(widget);
  disconnect(widget, SIGNAL(destroyed(QObject *)), this, SLOT(associatedWidgetDestroyed(QObject *)));
}

QList<QWidget *> ActionCollection::associatedWidgets() const {
  return _associatedWidgets;
}

void ActionCollection::clearAssociatedWidgets() {
  foreach(QWidget *widget, _associatedWidgets)
    foreach(QAction *action, actions())
      widget->removeAction(action);

  _associatedWidgets.clear();
}

void ActionCollection::associatedWidgetDestroyed(QObject *obj) {
  _associatedWidgets.removeAll(static_cast<QWidget *>(obj));
}

bool ActionCollection::unlistAction(QAction *action) {
  // This might be called with a partly destroyed QAction!

  int index = _actions.indexOf(action);
  if(index == -1) return false;

  QString name = action->objectName();
  _actionByName.remove(name);
  _actions.removeAt(index);

  // TODO: remove from ActionCategory if we ever get that

  return true;
}

#endif /* HAVE_KDE */
