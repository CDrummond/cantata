/*
 * Cantata
 *
 * Copyright (c) s2018 Craig Drummond <craig.p.drummond@gmail.com>
 *
 * ----
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef CATEGORIZEDVIEW_H
#define CATEGORIZEDVIEW_H

#include "kcategorizedview/kcategorizedview.h"
#include "treeview.h"

class QIcon;
class QMenu;
class KCategorizedSortFilterProxyModel;

class CategorizedView : public KCategorizedView
{
    Q_OBJECT

public:
    CategorizedView(QWidget *parent=nullptr);
    ~CategorizedView() override;

    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) override;
    bool haveSelectedItems() const;
    bool haveUnSelectedItems() const;
    void startDrag(Qt::DropActions supportedActions) override { TreeView::drag(supportedActions, this, selectedIndexes()); }
    void mouseReleaseEvent(QMouseEvent *event) override;
    QModelIndexList selectedIndexes() const override { return selectedIndexes(true); }
    QModelIndexList selectedIndexes(bool sorted) const;
    void setModel(QAbstractItemModel *m) override;
    void addDefaultAction(QAction *act);
    void setBackgroundImage(const QIcon &icon);
    void paintEvent(QPaintEvent *e) override;
    void installFilter(QObject *f) { eventFilter=f; installEventFilter(f); }
    QObject * filter() const { return eventFilter; }
    double zoom() const { return zoomLevel; }
    void setZoom(double l) { zoomLevel = l; }
    void setInfoText(const QString &i) { info=i; update(); }
    void setRootIndex(const QModelIndex &idx) override;
    QModelIndex rootIndex() const;
    QModelIndex indexAt(const QPoint &point) const override { return indexAt(point, false); }
    QModelIndex indexAt(const QPoint &point, bool ensureFromSource) const;
    QModelIndex mapFromSource(const QModelIndex &idx) const;
    void setPlain(bool plain);

private Q_SLOTS:
    void correctSelection();
    void showCustomContextMenu(const QPoint &pos);
    void checkDoubleClick(const QModelIndex &idx);
    void checkClicked(const QModelIndex &idx);
    void checkActivated(const QModelIndex &idx);

Q_SIGNALS:
    bool itemsSelected(bool);
    void itemDoubleClicked(const QModelIndex &idx);
    void itemClicked(const QModelIndex &idx);
    void itemActivated(const QModelIndex &idx);

private:
    void rowsInserted(const QModelIndex &parent, int start, int end) override;

private:
    QString info;
    QObject *eventFilter;
    QMenu *menu;
    QPixmap bgnd;
    double zoomLevel;
    KCategorizedSortFilterProxyModel *proxy;
};

#endif
