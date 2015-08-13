/*
 * Cantata
 *
 * Copyright (c) 2011-2015 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef APPLICATION_KDE_H
#define APPLICATION_KDE_H

#include <qglobal.h>
#include <KDE/KUniqueApplication>
class MainWindow;
class Application : public KUniqueApplication
{
    Q_OBJECT

public:
    static void initObjects();
    #ifdef Q_WS_X11
    Application(Display *display, Qt::HANDLE visual, Qt::HANDLE colormap);
    #endif
    Application();
    ~Application();

    int newInstance();

private Q_SLOTS:
    void mwDestroyed(QObject *obj);

private:
    MainWindow *w;
};

#endif
