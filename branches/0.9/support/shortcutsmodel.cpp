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

#include "shortcutsmodel.h"
#include "action.h"
#include "actioncollection.h"

static QString stripAcceleratorMarkers(const QString &label_) {
  QString label = label_;
  int p = 0;
  forever {
    p = label.indexOf('&', p);
    if(p < 0 || p + 1 >= label.length())
      break;

    if(label.at(p + 1).isLetterOrNumber() || label.at(p + 1) == '&')
      label.remove(p, 1);

    ++p;
  }
  return label;
}

ShortcutsModel::ShortcutsModel(const QHash<QString, ActionCollection *> &actionCollections, QObject *parent)
  : QAbstractItemModel(parent),
  _changedCount(0)
{
  for(int r = 0; r < actionCollections.values().count(); r++) {
    ActionCollection *coll = actionCollections.values().at(r);
    Item *item = new Item();
    item->row = r;
    item->collection = coll;
    for(int i = 0; i < coll->actions().count(); i++) {
      Action *action = qobject_cast<Action *>(coll->actions().at(i));
      if(!action)
        continue;
      Item *actionItem = new Item();
      actionItem->parentItem = item;
      actionItem->row = i;
      actionItem->collection = coll;
      actionItem->action = action;
      actionItem->shortcut = action->shortcut();
      item->actionItems.append(actionItem);
    }
    _categoryItems.append(item);
  }
}

ShortcutsModel::~ShortcutsModel() {
  qDeleteAll(_categoryItems);
}

QModelIndex ShortcutsModel::parent(const QModelIndex &child) const {
  if(!child.isValid())
    return QModelIndex();

  Item *item = static_cast<Item *>(child.internalPointer());
  Q_ASSERT(item);

  if(!item->parentItem)
    return QModelIndex();

  return createIndex(item->parentItem->row, 0, item->parentItem);
}

QModelIndex ShortcutsModel::index(int row, int column, const QModelIndex &parent) const {

  if(parent.isValid())
    return createIndex(row, column, static_cast<Item *>(parent.internalPointer())->actionItems.at(row));

  // top level category item
  return createIndex(row, column, _categoryItems.at(row));
}

int ShortcutsModel::columnCount(const QModelIndex &parent) const {
  return 2;
  /*if(!parent.isValid())
    return 2;

  Item *item = static_cast<Item *>(parent.internalPointer());
  Q_ASSERT(item);

  if(!item->parentItem)
    return 2;

  return 2;*/
}

int ShortcutsModel::rowCount(const QModelIndex &parent) const {
  if(!parent.isValid())
    return _categoryItems.count();

  Item *item = static_cast<Item *>(parent.internalPointer());
  Q_ASSERT(item);

  if(!item->parentItem)
    return item->actionItems.count();

  return 0;
}

QVariant ShortcutsModel::headerData(int section, Qt::Orientation orientation, int role) const {
  if(orientation != Qt::Horizontal || role != Qt::DisplayRole)
    return QVariant();
  switch(section) {
  case 0:
    return tr("Action");
  case 1:
    return tr("Shortcut");
  default:
    return QVariant();
  }
}

QVariant ShortcutsModel::data(const QModelIndex &index, int role) const {
  if(!index.isValid())
    return QVariant();

  Item *item = static_cast<Item *>(index.internalPointer());
  Q_ASSERT(item);

  if(!item->parentItem) {
    if(index.column() != 0)
      return QVariant();
    switch(role) {
    case Qt::DisplayRole:
      return item->collection->property("Category");
    default:
      return QVariant();
    }
  }

  Action *action = qobject_cast<Action *>(item->action);
  Q_ASSERT(action);

  switch(role) {
  case Qt::DisplayRole:
    switch(index.column()) {
    case 0:
      return stripAcceleratorMarkers(action->text());
    case 1:
      return item->shortcut.toString(QKeySequence::NativeText);
    default:
      return QVariant();
    }

  case Qt::DecorationRole:
    if(index.column() == 0)
      return action->icon();
    return QVariant();

  case ActionRole:
    return QVariant::fromValue<QObject *>(action);

  case DefaultShortcutRole:
    return action->shortcut(Action::DefaultShortcut);
  case ActiveShortcutRole:
    return item->shortcut;

  case IsConfigurableRole:
    return action->isShortcutConfigurable();

  default:
    return QVariant();
  }
}

bool ShortcutsModel::setData(const QModelIndex &index, const QVariant &value, int role) {
  if(role != ActiveShortcutRole)
    return false;

  if(!index.parent().isValid())
    return false;

  Item *item = static_cast<Item *>(index.internalPointer());
  Q_ASSERT(item);
  Q_ASSERT(item->action);

  QKeySequence newSeq = value.value<QKeySequence>();
  QKeySequence oldSeq = item->shortcut;
  QKeySequence storedSeq = item->action->shortcut(Action::ActiveShortcut);

  item->shortcut = newSeq;
  emit dataChanged(index, index.sibling(index.row(), 1));

  if(oldSeq == storedSeq && newSeq != storedSeq) {
    if(++_changedCount == 1)
      emit hasChanged(true);
  } else if(oldSeq != storedSeq && newSeq == storedSeq) {
    if(--_changedCount == 0)
      emit hasChanged(false);
  }

  return true;
}

void ShortcutsModel::load() {
  foreach(Item *catItem, _categoryItems) {
    foreach(Item *actItem, catItem->actionItems) {
      actItem->shortcut = actItem->action->shortcut(Action::ActiveShortcut);
    }
  }
  emit dataChanged(index(0, 1), index(rowCount()-1, 1));
  if(_changedCount != 0) {
    _changedCount = 0;
    emit hasChanged(false);
  }
}

void ShortcutsModel::commit() {
  foreach(Item *catItem, _categoryItems) {
    foreach(Item *actItem, catItem->actionItems) {
      actItem->action->setShortcut(actItem->shortcut, Action::ActiveShortcut);
    }
    catItem->collection->writeSettings();
  }
  if(_changedCount != 0) {
    _changedCount = 0;
    emit hasChanged(false);
  }
}

void ShortcutsModel::defaults() {
  for(int cat = 0; cat < rowCount(); cat++) {
    QModelIndex catidx = index(cat, 0);
    for(int act = 0; act < rowCount(catidx); act++) {
      QModelIndex actidx = index(act, 1, catidx);
      setData(actidx, actidx.data(DefaultShortcutRole), ActiveShortcutRole);
    }
  }
}
