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

#include "gnomemediakeys.h"
#include "mainwindow.h"
#include "mediakeysinterface.h"
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtCore/QCoreApplication>

static const char * constService = "org.gnome.SettingsDaemon";
static const char * constPath = "/org/gnome/SettingsDaemon/MediaKeys";

GnomeMediaKeys::GnomeMediaKeys(MainWindow *parent)
    : QObject(parent)
    , mw(parent)
    , iface(0)
{
}

void GnomeMediaKeys::setEnabled(bool en)
{
    if (en && !iface) {
        // Check if the service is available
        if (QDBusConnection::sessionBus().interface()->isServiceRegistered(constService)) {
            if (!iface) {
                iface = new OrgGnomeSettingsDaemonMediaKeysInterface(constService, constPath, QDBusConnection::sessionBus(), this);
            }

            QDBusPendingReply<> reply = iface->GrabMediaPlayerKeys(QCoreApplication::applicationName(), QDateTime::currentDateTime().toTime_t());
            QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
            connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher *)), this, SLOT(registerFinished(QDBusPendingCallWatcher*)));
        }
    } else if (!en && iface) {
        iface->ReleaseMediaPlayerKeys(QCoreApplication::applicationName());
        disconnect(iface, SIGNAL(MediaPlayerKeyPressed(QString,QString)), this, SLOT(keyPressed(QString,QString)));
        iface->deleteLater();
        iface=0;
    }
}

void GnomeMediaKeys::registerFinished(QDBusPendingCallWatcher *watcher)
{
    QDBusMessage reply = watcher->reply();
    watcher->deleteLater();

    if (QDBusMessage::ErrorMessage!=reply.type()) {
        connect(iface, SIGNAL(MediaPlayerKeyPressed(QString, QString)),  this, SLOT(keyPressed(QString,QString)));
    }
}

void GnomeMediaKeys::keyPressed(const QString &app, const QString &key)
{
    if (QCoreApplication::applicationName()!=app) {
        return;
    }
    if (QLatin1String("Play")==key) {
        mw->playPauseTrack();
    } else if (QLatin1String("Stop")==key) {
        mw->stopTrack();
    } else if (QLatin1String("Next")==key) {
        mw->nextTrack();
    } else if (QLatin1String("Previous")==key) {
        mw->prevTrack();
    }
}
