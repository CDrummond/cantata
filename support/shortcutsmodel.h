/***************************************************************************
 *   Copyright (C) 2010 by the Quassel Project                             *
 *   devel@quassel-irc.org                                                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3.                                           *
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
 ***************************************************************************/

#ifndef SHORTCUTSMODEL_H
#define SHORTCUTSMODEL_H

#include <QAbstractItemModel>
#include <QKeySequence>

class Action;
class ActionCollection;

//! Model that exposes the actions from one or more ActionCollections
/** This model takes one or more ActionCollections and exposes their actions as model items.
 *  Note that the ShortcutsModel will not react to changes in the ActionCollection (e.g. adding,
 *  removing actions), because it is supposed to be used after all actions being defined.
 */
class ShortcutsModel : public QAbstractItemModel {
    Q_OBJECT
public:
    enum Role {
        ActionRole = Qt::UserRole,
        DefaultShortcutRole,
        ActiveShortcutRole,
        IsConfigurableRole
    };

    ShortcutsModel(const QHash<QString, ActionCollection *> &actionCollections, QObject *parent = 0);
    ~ShortcutsModel();

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &child) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role = ActiveShortcutRole);

public slots:
    //! Load shortcuts from the ActionCollections
    /** Note that this will not rebuild the internal structure of the model, as we assume the
   *  ActionCollections to be static during the lifetime of the settingspage. This will merely
   *  re-read the shortcuts currently set in Quassel.
   */
    void load();

    //! Load default shortcuts from the ActionCollections
    /** Note that this will not rebuild the internal structure of the model, as we assume the
   *  ActionCollections to be static during the lifetime of the settingspage. This will update
   *  the model's state from the ActionCollections' defaults.
   */
    void defaults();

    //! Commit the model changes to the ActionCollections
    void commit();

    inline bool hasChanged() const { return _changedCount; }

signals:
    //! Reflects the difference between model contents and the ActionCollections we loaded this from
    void hasChanged(bool changed);

private:
    struct Item {
        inline Item() : row(-1), parentItem(0), collection(0), action(0) { }
        inline ~Item() { qDeleteAll(actionItems); }
        int row;
        Item *parentItem;
        ActionCollection *collection;
        Action *action;
        QKeySequence shortcut;
        QList<Item *> actionItems;
    };

    QList<Item *> _categoryItems;
    int _changedCount;
};

#endif // SHORTCUTSMODEL_H
