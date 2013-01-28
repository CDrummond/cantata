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
 * This is a subset of the API of KDE's KActionCollection.                 *
 ***************************************************************************/

#ifndef ACTIONCOLLECTION_H_
#define ACTIONCOLLECTION_H_

class Action;
class Icon;

#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KActionCollection>

class ActionCollection : public KActionCollection {
  Q_OBJECT

  public:
    explicit ActionCollection(QObject *parent) : KActionCollection(parent) {};

    static void setMainWidget(QWidget *w);
    static ActionCollection * get();
    Action * createAction(const QString &name, const QString &text, const char *icon=0, const QString &whatsThis=QString());
    Action * createAction(const QString &name, const QString &text, const Icon &icon, const QString &whatsThis=QString());
};

#else

#include <QList>
#include <QMap>
#include <QObject>
#include <QString>

class QWidget;
class QAction;

class ActionCollection : public QObject {
  Q_OBJECT

  public:
    explicit ActionCollection(QObject *parent);
    virtual ~ActionCollection();

    static void setMainWidget(QWidget *w);
    static ActionCollection * get();
    Action * createAction(const QString &name, const QString &text, const char *icon=0, const QString &whatsThis=QString());
    Action * createAction(const QString &name, const QString &text, const Icon &icon, const QString &whatsThis=QString());

    /// Clears the entire action collection, deleting all actions.
    void clear();

    /// Associate all action in this collection to the given \a widget.
    /** Not that this only adds all current actions in the collection to that widget;
     *  subsequently added actions won't be added automatically.
     */
    void associateWidget(QWidget *widget) const;

    /// Associate all actions in this collection to the given \a widget.
    /** Subsequently added actions will be automagically associated with this widget as well.
     */
    void addAssociatedWidget(QWidget *widget);

    void removeAssociatedWidget(QWidget *widget);
    QList<QWidget *> associatedWidgets() const;
    void clearAssociatedWidgets();

    void readSettings();
    void writeSettings() const;

    int count() const { return actions().count(); }
    bool isEmpty() const { return 0==actions().count(); }

    QAction *action(int index) const;
    QAction *action(const QString &name) const;
    QList<QAction *> actions() const;

    QAction *addAction(const QString &name, QAction *action);
    Action *addAction(const QString &name, Action *action);
    Action *addAction(const QString &name, const QObject *receiver = 0, const char *member = 0);
    void removeAction(QAction *action);
    QAction *takeAction(QAction *action);

    /// Create new action under the given name, add it to the collection and connect its triggered(bool) signal to the specified receiver.
    template<class ActionType>
    ActionType *add(const QString &name, const QObject *receiver = 0, const char *member = 0) {
      ActionType *a = new ActionType(this);
      if(receiver && member)
        connect(a, SIGNAL(triggered(bool)), receiver, member);
      addAction(name, a);
      return a;
    }

  signals:
    void inserted(QAction *action);
    void actionHovered(QAction *action);
    void actionTriggered(QAction *action);

  protected slots:
    virtual void connectNotify(const char *signal);
    virtual void slotActionTriggered();

  private slots:
    void slotActionHovered();
    void actionDestroyed(QObject *);
    void associatedWidgetDestroyed(QObject *);

  private:
    bool unlistAction(QAction *);
    QString configKey() const;

    QMap<QString, QAction *> _actionByName;
    QList<QAction *> _actions;
    QList<QWidget *> _associatedWidgets;

    bool _connectHovered;
    bool _connectTriggered;
};

#endif

#endif
