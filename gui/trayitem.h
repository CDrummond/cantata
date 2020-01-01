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

#ifndef TRAYITEM_H
#define TRAYITEM_H

#include <QObject>
#include <QSystemTrayIcon>
#include "support/icon.h"
class QMenu;
#include "config.h"

#ifdef QT_QTDBUS_FOUND
class Notify;
#endif
class MainWindow;
class QImage;
struct Song;
class Action;

class TrayItem : public QObject
{
    Q_OBJECT
public:
    TrayItem(MainWindow *p);
    ~TrayItem() override { }

    void showMessage(const QString &title, const QString &text, const QImage &img=QImage());
    void setup();
    #ifdef Q_OS_MAC
    bool isActive() const { return false; }
    void setIcon(const QIcon &) { }
    void setToolTip(const QString &, const QString &, const QString &) { }
    #else
    bool isActive() const { return nullptr!=trayItem; }
    void setIcon(const QIcon &icon) {
        if (trayItem) {
            trayItem->setIcon(icon);
        }
    }
    void setToolTip(const QString &iconName, const QString &title, const QString &subTitle) {
        if (trayItem) {
            Q_UNUSED(iconName)
            Q_UNUSED(subTitle)
            trayItem->setToolTip(title);
        }
    }
    #endif
    void songChanged(const Song &song, bool isPlaying);
    void updateConnections();
    void updateOutputs();

private Q_SLOTS:
    void trayItemClicked(QSystemTrayIcon::ActivationReason reason);

private:
    #ifndef Q_OS_MAC

    MainWindow *mw;
    QSystemTrayIcon *trayItem;
    QMenu *trayItemMenu;
    #ifdef QT_QTDBUS_FOUND
    Notify *notification;
    #endif
    Action *connectionsAction;
    Action *outputsAction;

    #endif
};

#endif
