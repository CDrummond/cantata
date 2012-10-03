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
 * Parts of this implementation are taken from KDE's kaction.cpp           *
 ***************************************************************************/

#include "action.h"
#include <QtGui/QApplication>

Action::Action(QObject *parent)
#ifdef ENABLE_KDE_SUPPORT
: KAction(parent)
#else
: QAction(parent)
#endif
{
  #ifndef ENABLE_KDE_SUPPORT
  init();
  #endif
}

Action::Action(const QString &text, QObject *parent, const QObject *receiver, const char *slot, const QKeySequence &shortcut)
#ifdef ENABLE_KDE_SUPPORT
: KAction(parent)
#else
: QAction(parent)
#endif
{
  #ifndef ENABLE_KDE_SUPPORT
  init();
  #endif
  setText(text);
  setShortcut(shortcut);
  if(receiver && slot)
    connect(this, SIGNAL(triggered()), receiver, slot);
}

Action::Action(const QIcon &icon, const QString &text, QObject *parent, const QObject *receiver, const char *slot, const QKeySequence &shortcut)
#ifdef ENABLE_KDE_SUPPORT
: KAction(parent)
#else
: QAction(parent)
#endif
{
  #ifndef ENABLE_KDE_SUPPORT
  init();
  #endif
  setIcon(icon);
  setText(text);
  setShortcut(shortcut);
  if(receiver && slot)
    connect(this, SIGNAL(triggered()), receiver, slot);
}

#ifndef ENABLE_KDE_SUPPORT
void Action::init() {
  connect(this, SIGNAL(triggered(bool)), this, SLOT(slotTriggered()));

  setProperty("isShortcutConfigurable", true);
}

void Action::slotTriggered() {
  emit triggered(QApplication::mouseButtons(), QApplication::keyboardModifiers());
}

bool Action::isShortcutConfigurable() const {
  return property("isShortcutConfigurable").toBool();
}

void Action::setShortcutConfigurable(bool b) {
  setProperty("isShortcutConfigurable", b);
}

QKeySequence Action::shortcut(ShortcutTypes type) const {
  Q_ASSERT(type);
  if(type == DefaultShortcut)
    return property("defaultShortcut").value<QKeySequence>();

  if(shortcuts().count()) return shortcuts().value(0);
  return QKeySequence();
}

void Action::setShortcut(const QShortcut &shortcut, ShortcutTypes type) {
  setShortcut(shortcut.key(), type);
}

void Action::setShortcut(const QKeySequence &key, ShortcutTypes type) {
  Q_ASSERT(type);

  if(type & DefaultShortcut)
    setProperty("defaultShortcut", key);

  if(type & ActiveShortcut)
    QAction::setShortcut(key);
}

#endif // !ENABLE_KDE_SUPPORT
