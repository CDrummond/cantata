
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

#include <QTimer>

#include "shortcutssettingspage.h"
#include "action.h"
#include "actioncollection.h"
#include "shortcutsmodel.h"

ShortcutsFilter::ShortcutsFilter(QObject *parent) : QSortFilterProxyModel(parent) {
  setDynamicSortFilter(true);
}

void ShortcutsFilter::setFilterString(const QString &filterString) {
  _filterString = filterString;
  invalidateFilter();
}

bool ShortcutsFilter::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const {
  if(!source_parent.isValid())
    return true;

  QModelIndex index = source_parent.model()->index(source_row, 0, source_parent);
  Q_ASSERT(index.isValid());
  if(!qobject_cast<Action *>(index.data(ShortcutsModel::ActionRole).value<QObject *>())->isShortcutConfigurable())
    return false;

  for(int col = 0; col < source_parent.model()->columnCount(source_parent); col++) {
    if(source_parent.model()->index(source_row, col, source_parent).data().toString().contains(_filterString, Qt::CaseInsensitive))
      return true;
  }
  return false;
}

/****************************************************************************/

ShortcutsSettingsPage::ShortcutsSettingsPage(const QHash<QString, ActionCollection *> &actionCollections, QWidget *parent)
  : QWidget(parent),
  _shortcutsModel(new ShortcutsModel(actionCollections, this)),
  _shortcutsFilter(new ShortcutsFilter(this))
{
  setupUi(this);

  _shortcutsFilter->setSourceModel(_shortcutsModel);
  shortcutsView->setModel(_shortcutsFilter);
  shortcutsView->expandAll();
  shortcutsView->resizeColumnToContents(0);
  shortcutsView->sortByColumn(0, Qt::AscendingOrder);
  if (1==_shortcutsModel->rowCount()) {
      shortcutsView->setIndentation(0);
      shortcutsView->setRootIndex(_shortcutsFilter->index(0, 0));
  }

  keySequenceWidget->setModel(_shortcutsModel);
  connect(keySequenceWidget, SIGNAL(keySequenceChanged(QKeySequence,QModelIndex)), SLOT(keySequenceChanged(QKeySequence,QModelIndex)));

  connect(shortcutsView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)), SLOT(setWidgetStates()));

  setWidgetStates();

  connect(useDefault, SIGNAL(clicked(bool)), SLOT(toggledCustomOrDefault()));
  connect(useCustom, SIGNAL(clicked(bool)), SLOT(toggledCustomOrDefault()));

//   connect(_shortcutsModel, SIGNAL(hasChanged(bool)), SLOT(setChangedState(bool)));

  // fugly, but directly setting it from the ctor doesn't seem to work
  QTimer::singleShot(0, searchEdit, SLOT(setFocus()));
}

QTreeView * ShortcutsSettingsPage::view()
{
    return shortcutsView;
}

void ShortcutsSettingsPage::setWidgetStates() {
  if(shortcutsView->currentIndex().isValid() && shortcutsView->currentIndex().parent().isValid()) {
    QKeySequence active = shortcutsView->currentIndex().data(ShortcutsModel::ActiveShortcutRole).value<QKeySequence>();
    QKeySequence def = shortcutsView->currentIndex().data(ShortcutsModel::DefaultShortcutRole).value<QKeySequence>();
    defaultShortcut->setText(def.isEmpty()? tr("None") : def.toString(QKeySequence::NativeText));
    actionBox->setEnabled(true);
    if(active == def) {
      useDefault->setChecked(true);
      keySequenceWidget->setKeySequence(QKeySequence());
    } else {
      useCustom->setChecked(true);
      keySequenceWidget->setKeySequence(active);
    }
  } else {
    defaultShortcut->setText(tr("None"));
    actionBox->setEnabled(false);
    useDefault->setChecked(true);
    keySequenceWidget->setKeySequence(QKeySequence());
  }
}

void ShortcutsSettingsPage::on_searchEdit_textChanged(const QString &text) {
  _shortcutsFilter->setFilterString(text);
}

void ShortcutsSettingsPage::keySequenceChanged(const QKeySequence &seq, const QModelIndex &conflicting) {
  if(conflicting.isValid())
    _shortcutsModel->setData(conflicting, QKeySequence(), ShortcutsModel::ActiveShortcutRole);

  QModelIndex rowIdx = _shortcutsFilter->mapToSource(shortcutsView->currentIndex());
  Q_ASSERT(rowIdx.isValid());
  _shortcutsModel->setData(rowIdx, seq, ShortcutsModel::ActiveShortcutRole);
  setWidgetStates();
}

void ShortcutsSettingsPage::toggledCustomOrDefault() {
  if(!shortcutsView->currentIndex().isValid())
    return;

  QModelIndex index = _shortcutsFilter->mapToSource(shortcutsView->currentIndex());
  Q_ASSERT(index.isValid());

  if(useDefault->isChecked()) {
    _shortcutsModel->setData(index, index.data(ShortcutsModel::DefaultShortcutRole));
  } else {
    _shortcutsModel->setData(index, QKeySequence());
  }
  setWidgetStates();
}

void ShortcutsSettingsPage::save() {
  _shortcutsModel->commit();
}
