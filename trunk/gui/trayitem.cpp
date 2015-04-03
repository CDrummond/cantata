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

#include "trayitem.h"
#include "config.h"
#ifdef QT_QTDBUS_FOUND
#include "dbus/notify.h"
#endif
#include "support/localize.h"
#include "mainwindow.h"
#include "settings.h"
#include "support/action.h"
#include "widgets/icons.h"
#include "mpd-interface/song.h"
#include "stdactions.h"
#include "support/utils.h"
#include "currentcover.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KMenu>
#include <QPixmap>
#endif
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
    bool eventFilter(QObject *obj, QEvent *event)
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
    , trayItem(0)
    , trayItemMenu(0)
    #ifdef QT_QTDBUS_FOUND
    , notification(0)
    #endif
    , connectionsAction(0)
    , outputsAction(0)
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

void TrayItem::setup()
{
    #ifndef Q_OS_MAC
    if (!Settings::self()->useSystemTray()) {
        if (trayItem) {
            #ifndef ENABLE_KDE_SUPPORT
            trayItem->setVisible(false);
            #endif
            trayItem->deleteLater();
            trayItem=0;
            trayItemMenu->deleteLater();
            trayItemMenu=0;
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

    #ifdef ENABLE_KDE_SUPPORT
    trayItem = new KStatusNotifierItem(this);
    trayItem->setCategory(KStatusNotifierItem::ApplicationStatus);
    trayItem->setTitle(i18n("Cantata"));
    trayItem->setIconByName(QIcon::hasThemeIcon("cantata-panel") ? QLatin1String("cantata-panel") : QLatin1String("cantata"));
    trayItem->setToolTip("cantata", i18n("Cantata"), QString());
    trayItem->setStandardActionsEnabled(false);

    trayItemMenu = new KMenu(0);
    trayItemMenu->addAction(StdActions::self()->prevTrackAction);
    trayItemMenu->addAction(StdActions::self()->playPauseTrackAction);
    trayItemMenu->addAction(StdActions::self()->stopPlaybackAction);
    trayItemMenu->addAction(StdActions::self()->stopAfterCurrentTrackAction);
    trayItemMenu->addAction(StdActions::self()->nextTrackAction);
    trayItem->setContextMenu(trayItemMenu);
    trayItem->setStatus(KStatusNotifierItem::Active);
    trayItemMenu->addSeparator();
    trayItemMenu->addAction(connectionsAction);
    trayItemMenu->addAction(outputsAction);
    trayItemMenu->addSeparator();
    trayItemMenu->addAction(mw->restoreAction);
    trayItemMenu->addSeparator();
    trayItemMenu->addAction(mw->quitAction);

    connect(trayItem, SIGNAL(scrollRequested(int, Qt::Orientation)), this, SLOT(trayItemScrollRequested(int, Qt::Orientation)));
    connect(trayItem, SIGNAL(secondaryActivateRequested(const QPoint &)), mw, SLOT(playPauseTrack()));
    connect(trayItem, SIGNAL(activateRequested(bool, const QPoint &)), this, SLOT(clicked()));
    #else
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        trayItem = NULL;
        return;
    }

    trayItem = new QSystemTrayIcon(this);
    trayItem->installEventFilter(new VolumeSliderEventHandler(this));
    trayItemMenu = new QMenu(0);
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
    trayItemMenu->addAction(mw->quitAction);
    trayItem->setContextMenu(trayItemMenu);
    QIcon icon=QIcon::fromTheme(QIcon::hasThemeIcon("cantata-panel") ? "cantata-panel" : "cantata");
    #if !defined Q_OS_MAC && !defined Q_OS_WIN
    // Bug: 660 If installed to non-standard folder, QIcon::fromTheme does not seem to find icon. Therefore
    // add icon files here...
    if (icon.isNull()) {
        QStringList sizes=QStringList() << "16" << "22" << "24" << "32" << "48" << "64";
        foreach (const QString &s, sizes) {
            icon.addFile(QLatin1String(ICON_INSTALL_PREFIX "/")+s+QLatin1Char('x')+s+QLatin1String("/apps/cantata.png"));
        }

        icon.addFile(QLatin1String(ICON_INSTALL_PREFIX "/scalable/apps/cantata.svg"));
    }
    #endif
    trayItem->setIcon(icon);
    trayItem->setToolTip(i18n("Cantata"));
    trayItem->show();
    connect(trayItem, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(trayItemClicked(QSystemTrayIcon::ActivationReason)));
    #endif
    #endif
}

#ifdef ENABLE_KDE_SUPPORT
void TrayItem::clicked()
{
    if (mw->isHidden()) {
        mw->restoreWindow();
    } else {
        mw->hideWindow();
    }
}

void TrayItem::trayItemScrollRequested(int delta, Qt::Orientation orientation)
{
    if (Qt::Vertical==orientation) {
        if (delta>0) {
            StdActions::self()->increaseVolumeAction->trigger();
        } else if(delta<0) {
            StdActions::self()->decreaseVolumeAction->trigger();
        }
    }
}
#else
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
#endif

void TrayItem::songChanged(const Song &song, bool isPlaying)
{
    #ifdef Q_OS_MAC
    if (Settings::self()->showPopups()) {
        bool useable=song.isStandardStream()
                        ? !song.title.isEmpty() && !song.name().isEmpty()
                        : !song.title.isEmpty() && !song.artist.isEmpty() && !song.album.isEmpty();
        if (useable) {
            QString text=song.describe(false);
            if (song.time>0) {
                text+=QLatin1String(" - ")+Utils::formatTime(song.time);
            }
            MacNotify::showMessage(i18n("Now playing"), text, CurrentCover::self()->image());
        }
    }
    #else
    if (Settings::self()->showPopups() || trayItem) {
        bool useable=song.isStandardStream()
                        ? !song.title.isEmpty() && !song.name().isEmpty()
                        : !song.title.isEmpty() && !song.artist.isEmpty() && !song.album.isEmpty();
        if (useable) {
            QString text=song.describe(false);
            if (song.time>0) {
                text+=QLatin1String(" - ")+Utils::formatTime(song.time);
            }

            if (trayItem) {
                #ifdef ENABLE_KDE_SUPPORT
                trayItem->setToolTip("cantata", i18n("Cantata"), text);

                // Use the cover as icon pixmap.
                if (CurrentCover::self()->isValid() && !CurrentCover::self()->image().isNull()) {
                    trayItem->setToolTipIconByPixmap(QPixmap::fromImage(CurrentCover::self()->image()));
                }
                #elif defined Q_OS_WIN || defined Q_OS_MAC || !defined QT_QTDBUS_FOUND
                trayItem->setToolTip(i18n("Cantata")+"\n\n"+text);
                // The pure Qt implementation needs both the tray icon and the setting checked.
                if (Settings::self()->showPopups() && isPlaying) {
                    trayItem->showMessage(i18n("Now playing"), text, QSystemTrayIcon::Information, 5000);
                }
                #else
                trayItem->setToolTip(i18n("Cantata")+"\n\n"+text);
                #endif // ENABLE_KDE_SUPPORT
            }
            #ifdef QT_QTDBUS_FOUND
            if (Settings::self()->showPopups() && isPlaying) {
                if (!notification) {
                    notification=new Notify(this);
                }
                notification->show(i18n("Now playing"), text, CurrentCover::self()->image());
            }
            #endif
        } else if (trayItem) {
            #ifdef ENABLE_KDE_SUPPORT
            trayItem->setToolTip("cantata", i18n("Cantata"), QString());
            #else
            trayItem->setToolTip(i18n("Cantata"));
            #endif
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
            to->setMenu(new QMenu(0));
        }
        QMenu *m=to->menu();
        m->clear();

        foreach (QAction *act, from->menu()->actions()) {
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
