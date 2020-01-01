/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef OSXSTYLE_H
#define OSXSTYLE_H

#include <QStyleOption>
#include <QMap>
#include <QObject>

class QPalette;
class QTreeWidget;
class QPainter;
class QColor;
class QWidget;
class QMenu;
class QAction;
class QMainWindow;
class QWindow;
class Action;

class OSXStyle : public QObject
{
    Q_OBJECT

public:
    static OSXStyle * self();

    OSXStyle();
    const QPalette & viewPalette();
    void drawSelection(QStyleOptionViewItem opt, QPainter *painter, double opacity);
    QColor monoIconColor();

    void initWindowMenu(QMainWindow *mw);

public:
    void addWindow(QWidget *w);
    void removeWindow(QWidget *w);

private Q_SLOTS:
    void showWindow();
    void windowTitleChanged();
    void focusWindowChanged(QWindow *win);
    void closeWindow();
    void minimizeWindow();
    void zoomWindow();

private:
    QWidget * currentWindow();
    void controlActions(QWidget *w);
    QTreeWidget * viewWidget();

private:
    QTreeWidget *view;
    QMenu *windowMenu;
    QMap<QWidget *, QAction *> actions;
    QAction *closeAct;
    QAction *minAct;
    QAction *zoomAct;

    friend class Dialog;
};

#endif // OSXSTYLE_H
