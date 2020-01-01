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

#include "trayitem.h"
#include "config.h"
#ifdef QT_QTDBUS_FOUND
#include "dbus/notify.h"
#endif
#include "mainwindow.h"
#include "settings.h"
#include "support/action.h"
#include "widgets/icons.h"
#include "mpd-interface/song.h"
#include "stdactions.h"
#include "support/utils.h"
#include "currentcover.h"
#include <QWheelEvent>
#include <QMenu>
#ifdef Q_OS_MAC
#include "mac/macnotify.h"
#endif

#ifndef Q_OS_MAC
class VolumeSliderEventHandler : public QObject
{
public:
    VolumeSliderEventHandler(QObject *p) : QObject(p) { }
protected:
    bool eventFilter(QObject *obj, QEvent *event) override
    {
        if (QEvent::Wheel==event->type()) {
            int numDegrees = static_cast<QWheelEvent *>(event)->delta() / 8;
            int numSteps = numDegrees / 15;
            if (numSteps > 0) {
                for (int i = 0; i < numSteps; ++i) {
                    StdActions::self()->increaseVolumeAction->trigger();
                }
            } else {
                for (int i = 0; i > numSteps; --i) {
                    StdActions::self()->decreaseVolumeAction->trigger();
                }
            }
            return true;
        }

        return QObject::eventFilter(obj, event);
    }
};
#endif

TrayItem::TrayItem(MainWindow *p)
    : QObject(p)
    #ifndef Q_OS_MAC
    , mw(p)
    , trayItem(nullptr)
    , trayItemMenu(nullptr)
    #ifdef QT_QTDBUS_FOUND
    , notification(nullptr)
    #endif
    , connectionsAction(nullptr)
    , outputsAction(nullptr)
    #endif
{
}

void TrayItem::showMessage(const QString &title, const QString &text, const QImage &img)
{
    #ifdef Q_OS_MAC
    MacNotify::showMessage(title, text, img);
    #elif defined QT_QTDBUS_FOUND
    if (!notification) {
        notification=new Notify(this);
    }
    notification->show(title, text, img);
    #else
    Q_UNUSED(img)
    if (trayItem) {
        trayItem->showMessage(title, text, QSystemTrayIcon::Information, 5000);
    }
    #endif
}

static Action * copyAction(Action *orig)
{
    Action *newAction=new Action(orig->parent());
    newAction->setText(Utils::strippedText(orig->text()));
    newAction->setIcon(orig->icon());
    QObject::connect(newAction, SIGNAL(triggered()), orig, SIGNAL(triggered()));
    QObject::connect(newAction, SIGNAL(triggered(bool)), orig, SIGNAL(triggered(bool)));
    return newAction;
}

void TrayItem::setup()
{
    #ifndef Q_OS_MAC
    if (!Settings::self()->useSystemTray() || !Utils::useSystemTray()) {
        if (trayItem) {
            trayItem->setVisible(false);
            trayItem->deleteLater();
            trayItem=nullptr;
            trayItemMenu->deleteLater();
            trayItemMenu=nullptr;
        }
        return;
    }

    if (trayItem) {
        return;
    }

    #ifndef Q_OS_MAC
    connectionsAction=new Action(Utils::strippedText(mw->connectionsAction->text()), this);
    connectionsAction->setVisible(false);
    outputsAction=new Action(Utils::strippedText(mw->outputsAction->text()), this);
    outputsAction->setVisible(false);
    updateConnections();
    updateOutputs();
    #endif

    // What systems DONT have a system tray? Also, isSytemTrayAvailable is checked in config dialog, so
    // useSystemTray should not be set if there is none.
    // Checking here seems to cause the icon not to appear if Cantata is autostarted in Plasma5 - #759
    //if (!QSystemTrayIcon::isSystemTrayAvailable()) {
    //    return;
    //}

    trayItem = new QSystemTrayIcon(this);
    trayItem->installEventFilter(new VolumeSliderEventHandler(this));
    trayItemMenu = new QMenu(nullptr);
    trayItemMenu->addAction(StdActions::self()->prevTrackAction);
    trayItemMenu->addAction(StdActions::self()->playPauseTrackAction);
    trayItemMenu->addAction(StdActions::self()->stopPlaybackAction);
    trayItemMenu->addAction(StdActions::self()->stopAfterCurrentTrackAction);
    trayItemMenu->addAction(StdActions::self()->nextTrackAction);
    #ifndef Q_OS_MAC
    trayItemMenu->addSeparator();
    trayItemMenu->addAction(connectionsAction);
    trayItemMenu->addAction(outputsAction);
    #endif
    trayItemMenu->addSeparator();
    trayItemMenu->addAction(mw->restoreAction);
    trayItemMenu->addSeparator();
    trayItemMenu->addAction(copyAction(mw->quitAction));
    trayItem->setContextMenu(trayItemMenu);
    #if defined Q_OS_MAC || defined Q_OS_WIN
    QIcon icon;
    icon.addFile(CANTATA_SYS_ICONS_DIR+"cantata.png");
    #else
    QIcon icon=QIcon::fromTheme(Utils::Gnome==Utils::currentDe() ? "cantata-symbolic" : "cantata");
    // Bug: 660 If installed to non-standard folder, QIcon::fromTheme does not seem to find icon. Therefore
    // add icon files here...
    if (icon.isNull()) {
        QStringList sizes=QStringList() << "16" << "22" << "24" << "32" << "48" << "64";
        for (const QString &s: sizes) {
            icon.addFile(QLatin1String(ICON_INSTALL_PREFIX "/")+s+QLatin1Char('x')+s+QLatin1String("/apps/cantata.png"));
        }

        icon.addFile(QLatin1String(ICON_INSTALL_PREFIX "/scalable/apps/cantata.svg"));
    }
    #endif
    trayItem->setIcon(icon);
    trayItem->setToolTip(tr("Cantata"));
    trayItem->show();
    connect(trayItem, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(trayItemClicked(QSystemTrayIcon::ActivationReason)));
    #endif
}

void TrayItem::trayItemClicked(QSystemTrayIcon::ActivationReason reason)
{
    #ifdef Q_OS_MAC
    Q_UNUSED(reason)
    #else
    switch (reason) {
    case QSystemTrayIcon::Trigger:
        if (mw->isHidden()) {
            mw->restoreWindow();
        } else {
            mw->hideWindow();
        }
        break;
    case QSystemTrayIcon::MiddleClick:
        mw->playPauseTrack();
        break;
    default:
        break;
    }
    #endif
}

void TrayItem::songChanged(const Song &song, bool isPlaying)
{
    #ifdef Q_OS_MAC
    if (Settings::self()->showPopups()) {
        bool useable=song.isStandardStream()
                        ? !song.title.isEmpty() && !song.name().isEmpty()
                        : !song.title.isEmpty() && !song.artist.isEmpty() && !song.album.isEmpty();
        if (useable) {
            MacNotify::showMessage(song.mainText(), song.subText(), CurrentCover::self()->image());
        }
    }
    #else
    if (Settings::self()->showPopups() || trayItem) {
        bool useable=song.isStandardStream()
                        ? !song.title.isEmpty() && !song.name().isEmpty()
                        : !song.title.isEmpty() && !song.artist.isEmpty() && !song.album.isEmpty();
        if (useable) {
            if (trayItem) {
                trayItem->setToolTip(QLatin1String("Cantata\n\n")+song.mainText()+"\n"+song.subText());
                #if defined Q_OS_WIN || defined Q_OS_MAC || !defined QT_QTDBUS_FOUND
                // The pure Qt implementation needs both the tray icon and the setting checked.
                if (Settings::self()->showPopups() && isPlaying) {
                    trayItem->showMessage(song.mainText(), song.subText(), QSystemTrayIcon::Information, 5000);
                }
                #endif
            }
            #ifdef QT_QTDBUS_FOUND
            if (Settings::self()->showPopups() && isPlaying) {
                if (!notification) {
                    notification=new Notify(this);
                }
                notification->show(song.mainText(), song.subText(), CurrentCover::self()->image(), Notify::LowUrgency);
            }
            #endif
        } else if (trayItem) {
            trayItem->setToolTip(QLatin1String("Cantata"));
        }
    }
    #endif
}

#ifndef Q_OS_MAC
static void copyMenu(Action *from, Action *to)
{
    if (!to) {
        return;
    }
    to->setVisible(from->isVisible());
    if (to->isVisible()) {
        if (!to->menu()) {
            to->setMenu(new QMenu(nullptr));
        }
        QMenu *m=to->menu();
        m->clear();

        for (QAction *act: from->menu()->actions()) {
            m->addAction(act);
        }
    }
}
#endif

void TrayItem::updateConnections()
{
    #ifndef Q_OS_MAC
    copyMenu(mw->connectionsAction, connectionsAction);
    #endif
}

void TrayItem::updateOutputs()
{
    #ifndef Q_OS_MAC
    copyMenu(mw->outputsAction, outputsAction);
    #endif
}

#include "moc_trayitem.cpp"
