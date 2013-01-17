/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef TRAYITEM_H
#define TRAYITEM_H

#include <QtCore/QObject>

#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KStatusNotifierItem>
class KMenu;
class KNotification;
#else
#include <QtGui/QSystemTrayIcon>
#include "icon.h"
class QMenu;
class Notify;
#endif
class MainWindow;
class Song;

class TrayItem : public QObject
{
    Q_OBJECT
public:
    TrayItem(MainWindow *p);
    virtual ~TrayItem() { }

    void setup();
    bool isActive() const { return 0!=trayItem; }
    #ifdef ENABLE_KDE_SUPPORT
    void setIconByName(const QString &name) {
        if (trayItem) {
            trayItem->setIconByName(name);
        }
    }
    #else
    void setIcon(const QIcon &icon) {
        if (trayItem) {
            trayItem->setIcon(icon);
        }
    }
    #endif
    void setToolTip(const QString &iconName, const QString &title, const QString &subTitle) {
        if (trayItem) {
            #ifdef ENABLE_KDE_SUPPORT
            trayItem->setToolTip(iconName, title, subTitle);
            #else
            Q_UNUSED(iconName)
            Q_UNUSED(subTitle)
            trayItem->setToolTip(title);
            #endif
        }
    }
    void songChanged(const Song &song, bool isPlaying);

private Q_SLOTS:
    #ifdef ENABLE_KDE_SUPPORT
    void clicked();
    void trayItemScrollRequested(int delta, Qt::Orientation orientation);
    void notificationClosed();
    #else
    void trayItemClicked(QSystemTrayIcon::ActivationReason reason);
    #endif

private:
    MainWindow *mw;
    #ifdef ENABLE_KDE_SUPPORT
    KStatusNotifierItem *trayItem;
    KMenu *trayItemMenu;
    KNotification *notification;
    #else
    QSystemTrayIcon *trayItem;
    QMenu *trayItemMenu;
    Notify *notification;
    #endif
};

#endif
