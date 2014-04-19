/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef APPLICATION_H
#define APPLICATION_H

#include "config.h"
#include <qglobal.h>
#ifdef ENABLE_KDE_SUPPORT
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
#elif defined Q_OS_WIN || defined WIN32 || defined Q_OS_MAC || defined __APPLE__   // moc does not seem to see Q_OS_WIN/Q_OS_MAC,, but will see WIN32/__APPLE__ :-(
#include "qtsingleapplication/qtsingleapplication.h"
class Application : public QtSingleApplication
{
    Q_OBJECT

public:
    static void initObjects();
    Application(int &argc, char **argv);
    virtual ~Application() { }

    #if defined Q_OS_WIN || defined WIN32
    bool winEventFilter(MSG *msg, long *result);
    #endif
    bool start();
    void loadFiles();

private:
    void load(const QStringList &files);

private Q_SLOTS:
    void message(const QString &m);

Q_SIGNALS:
    void reconnect();
};
#else
#include <QApplication>
class Application : public QApplication
{
public:
    static void initObjects();
    Application(int &argc, char **argv);
    virtual ~Application() { }

    bool start();
    void loadFiles();
    void setupIconTheme();
};
#endif

#endif
