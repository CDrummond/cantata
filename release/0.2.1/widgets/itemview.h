/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef ITEMVIEW_H
#define ITEMVIEW_H

#include "ui_itemview.h"
class QAction;

#ifdef ENABLE_KDE_SUPPORT
class KPixmapSequenceOverlayPainter;
#endif

class ProxyModel;

class ItemView : public QWidget, public Ui::ItemView
{
    Q_OBJECT
public:
    enum Mode
    {
        Mode_Tree,
        Mode_List,
        Mode_IconTop,

        Mode_Count
    };

    enum Role
    {
        Role_ImageSize = Qt::UserRole+256,
        Role_SubText,
        Role_Image
    };

    ItemView(QWidget *p);
    virtual ~ItemView();

    void init(QAction *a1, QAction *a2);
    void addAction(QAction *act);
    void setMode(Mode m);
    Mode viewMode() const { return mode; }
    void setLevel(int level, bool haveChildren=true);
    QAbstractItemView * view() const;
    void setModel(ProxyModel *m);
    void clearSelection() { view()->selectionModel()->clearSelection(); }
    QModelIndexList selectedIndexes() const;
    QString searchText() const;
    void setTopText(const QString &text);
    void setUniformRowHeights(bool v);
    void setAcceptDrops(bool v);
    void setDragDropOverwriteMode(bool v);
    void setDragDropMode(QAbstractItemView::DragDropMode v);
    void setGridSize(const QSize &sz);
    QSize gridSize() const { return listView->gridSize(); }
    void update() { Mode_Tree==mode ? treeView->update() : listView->update(); }
    void setDeleteAction(QAction *act);

public Q_SLOTS:
    void showSpinner();
    void hideSpinner();

Q_SIGNALS:
    void searchItems();
    void itemsSelected(bool);
    void doubleClicked(const QModelIndex &);

private Q_SLOTS:
    void backActivated();
    void itemClicked(const QModelIndex &index);
    void itemActivated(const QModelIndex &index);

private:
    QAction * getAction(const QModelIndex &index);

private:
    ProxyModel *itemModel;
    QAction *backAction;
    QAction *act1;
    QAction *act2;
    int currentLevel;
    Mode mode;
    QString topText;
    QString prevText;
    QModelIndex prevTopIndex;
    QSize iconGridSize;
    QSize listGridSize;
    #ifdef ENABLE_KDE_SUPPORT
    bool spinnerActive;
    KPixmapSequenceOverlayPainter *spinner;
    #endif
};

#endif
