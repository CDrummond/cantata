/*
 * Cantata
 *
 * Copyright (c) 2011-2016 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "singleapplication.h"
#include "mainwindow.h"
#include "mpd-interface/mpdconnection.h"

SingleApplication::SingleApplication(int &argc, char **argv)
    : QtSingleApplication(argc, argv)
{
    connect(this, SIGNAL(messageReceived(const QString &)), SLOT(message(const QString &)));
    connect(this, SIGNAL(reconnect()), MPDConnection::self(), SLOT(reconnect()));
}

bool SingleApplication::start()
{
    if (isRunning()) {
        QStringList args(arguments());
        if (args.count()>1) {
            args.takeAt(0);
            sendMessage(args.join("\n"));
        } else {
            sendMessage(QString());
        }
        return false;
    }

    return true;
}

void SingleApplication::message(const QString &msg)
{
    if (!msg.isEmpty()) {
        load(msg.split("\n"));
    }
    MainWindow *mw=qobject_cast<MainWindow *>(activationWindow());
    if (mw) {
        mw->restoreWindow();
    }
}

void SingleApplication::loadFiles()
{
    QStringList args(arguments());
    if (args.count()>1) {
        args.takeAt(0);
        load(args);
    }
}

void SingleApplication::load(const QStringList &files)
{
    if (files.isEmpty()) {
        return;
    }

    QStringList urls;
    foreach (const QString &f, files) {
        urls.append(f);
    }
    if (!urls.isEmpty()) {
        MainWindow *mw=qobject_cast<MainWindow *>(activationWindow());
        if (mw) {
            mw->load(urls);
        }
    }
}

