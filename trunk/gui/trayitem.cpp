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
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KMenu>
#include <QPixmap>
#endif
#ifdef QT_QTDBUS_FOUND
#include "notify.h"
#endif
#include "localize.h"
#include "mainwindow.h"
#include "settings.h"
#include "action.h"
#include "icons.h"
#include "song.h"
#include "stdactions.h"
#include "utils.h"
#include "currentcover.h"

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

TrayItem::TrayItem(MainWindow *p)
    : QObject(p)
    , mw(p)
    , trayItem(0)
    , trayItemMenu(0)
    #ifdef QT_QTDBUS_FOUND
    , notification(0)
    #endif
{
}

void TrayItem::setup()
{
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

    #if !defined Q_OS_WIN32 && !defined Q_OS_MAC
    QString iconFile=QLatin1String(INSTALL_PREFIX"/share/")+QCoreApplication::applicationName()+QLatin1String("/icons/")+QIcon::themeName()+
                     QLatin1String("/systray.svg");
    #endif

    #ifdef ENABLE_KDE_SUPPORT
    trayItem = new KStatusNotifierItem(this);
    trayItem->setCategory(KStatusNotifierItem::ApplicationStatus);
    trayItem->setTitle(i18n("Cantata"));
    trayItem->setIconByName(!iconFile.isEmpty() && QFile::exists(iconFile) ? iconFile : QLatin1String("cantata"));
    trayItem->setToolTip("cantata", i18n("Cantata"), QString());

    trayItemMenu = new KMenu(0);
    trayItemMenu->addAction(StdActions::self()->prevTrackAction);
    trayItemMenu->addAction(StdActions::self()->playPauseTrackAction);
    trayItemMenu->addAction(StdActions::self()->stopPlaybackAction);
    trayItemMenu->addAction(StdActions::self()->stopAfterCurrentTrackAction);
    trayItemMenu->addAction(StdActions::self()->nextTrackAction);
    trayItem->setContextMenu(trayItemMenu);
    trayItem->setStatus(KStatusNotifierItem::Active);
    trayItemMenu->addSeparator();
    trayItemMenu->addAction(mw->restoreAction);
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
    trayItemMenu->addSeparator();
    trayItemMenu->addAction(mw->restoreAction);
    trayItemMenu->addSeparator();
    trayItemMenu->addAction(mw->quitAction);
    trayItem->setContextMenu(trayItemMenu);
    Icon icon;
    #if !defined Q_OS_WIN32 && !defined Q_OS_MAC
    if (QFile::exists(iconFile)) {
        icon.addFile(iconFile);
    }
    #endif
    trayItem->setIcon(icon.isNull() ? Icons::self()->appIcon : icon);
    trayItem->setToolTip(i18n("Cantata"));
    trayItem->show();
    connect(trayItem, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(trayItemClicked(QSystemTrayIcon::ActivationReason)));
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
}
#endif

void TrayItem::songChanged(const Song &song, bool isPlaying)
{
    if (Settings::self()->showPopups() || trayItem) {
        bool useable=song.isStandardStream()
                        ? !song.title.isEmpty() && !song.name.isEmpty()
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
                #elif defined Q_OS_WIN || !defined QT_QTDBUS_FOUND
                trayItem->setToolTip(i18n("Cantata")+"\n\n"+text);
                // The pure Qt implementation needs both the tray icon and the setting checked.
                if (Settings::self()->showPopups() && isPlaying) {
                    trayItem->showMessage(i18n("Cantata"), text, QSystemTrayIcon::Information, 5000);
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
}
