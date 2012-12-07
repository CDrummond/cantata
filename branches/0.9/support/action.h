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
 * Parts of this API have been shamelessly stolen from KDE's kaction.h     *
 ***************************************************************************/

#ifndef ACTION_H_
#define ACTION_H_

#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KAction>
class Action : public KAction {
  Q_OBJECT

  public:
    explicit Action(QObject *parent);
    Action(const QString &text, QObject *parent, const QObject *receiver = 0, const char *slot = 0, const QKeySequence &shortcut = 0);
    Action(const QIcon &icon, const QString &text, QObject *parent, const QObject *receiver = 0, const char *slot = 0, const QKeySequence &shortcut = 0);
};
#else
#include <QtGui/QShortcut>
#include <QtGui/QAction>

/// A specialized QWidgetAction, enhanced by some KDE features
/** This declares/implements a subset of KAction's API (notably we've left out global shortcuts
 *  for now), which should make it easy to plug in KDE support later on.
 */
class Action : public QAction {
  Q_OBJECT

  Q_PROPERTY(QKeySequence shortcut READ shortcut WRITE setShortcut)
  Q_PROPERTY(bool shortcutConfigurable READ isShortcutConfigurable WRITE setShortcutConfigurable)
  Q_FLAGS(ShortcutType)

  public:
    enum ShortcutType {
      ActiveShortcut = 0x01,
      DefaultShortcut = 0x02
    };
    Q_DECLARE_FLAGS(ShortcutTypes, ShortcutType)

    explicit Action(QObject *parent);
    Action(const QString &text, QObject *parent, const QObject *receiver = 0, const char *slot = 0, const QKeySequence &shortcut = 0);
    Action(const QIcon &icon, const QString &text, QObject *parent, const QObject *receiver = 0, const char *slot = 0, const QKeySequence &shortcut = 0);

    QKeySequence shortcut(ShortcutTypes types = ActiveShortcut) const;
    void setShortcut(const QShortcut &shortcut, ShortcutTypes type = ShortcutTypes(ActiveShortcut | DefaultShortcut));
    void setShortcut(const QKeySequence &shortcut, ShortcutTypes type = ShortcutTypes(ActiveShortcut | DefaultShortcut));

    bool isShortcutConfigurable() const;
    void setShortcutConfigurable(bool configurable);

  signals:
    void triggered(Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers);

  private:
    void init();

  private slots:
    void slotTriggered();
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Action::ShortcutTypes)
#endif

#endif
