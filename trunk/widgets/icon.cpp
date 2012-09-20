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

#include "icon.h"
#include <QtGui/QToolButton>
#include <QtGui/QApplication>
#include <QtGui/QPixmap>
#include <QtGui/QFont>
#include <QtGui/QPainter>
#include <QtCore/QDir>
#include <math.h>

static QPixmap createSingleIconPixmap(int size, QColor &col, double opacity)
{
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    QFont font(QLatin1String("sans"));
    font.setBold(false);
    font.setItalic(false);
    font.setPixelSize(size*0.9);
    p.setFont(font);
    p.setPen(col);
    p.setOpacity(opacity);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.drawText(QRect(0, 1, size, size), QLatin1String("1"), QTextOption(Qt::AlignHCenter|Qt::AlignVCenter));
    p.drawText(QRect(1, 1, size, size), QLatin1String("1"), QTextOption(Qt::AlignHCenter|Qt::AlignVCenter));
    p.drawText(QRect(-1, 1, size, size), QLatin1String("1"), QTextOption(Qt::AlignHCenter|Qt::AlignVCenter));
    p.setRenderHint(QPainter::Antialiasing, false);
    p.end();
    return pix;
}

static QPixmap createConsumeIconPixmap(int size, QColor &col, double opacity)
{
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setPen(QPen(col, 1.5));
    p.setOpacity(opacity);
    p.setRenderHint(QPainter::Antialiasing, true);
    QRectF rect(2.5, 2.5, size-4, size-4);
    double distanceX=fabs(cos(35.0))*(rect.width()/2);
    double distanceY=fabs(sin(35.0))*(rect.height()/2);
    double midX=rect.x()+(rect.width()/2);
    double midY=rect.y()+(rect.height()/2);
    p.drawArc(rect, 40*16, 280*16);
    p.drawLine(midX, midY, midX+distanceX, midY-distanceY);
    p.drawLine(midX, midY, midX+distanceX, midY+distanceY);
    p.drawPoint(midX, rect.y()+rect.height()/4);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.end();
    return pix;
}

static QIcon createIcon(bool isSingle)
{
    QIcon icon;
    QColor stdColor=QColor(QApplication::palette().color(QPalette::Active, QPalette::WindowText));
    if (stdColor==Qt::white) {
        stdColor=QColor(200, 200, 200);
    } else if (stdColor.red()<128 && stdColor.green()<128 && stdColor.blue()<128 &&
               stdColor.red()==stdColor.green() && stdColor.green()==stdColor.blue()) {
        stdColor=Qt::black;
    }
    QColor highlightColor=stdColor.red()<100 ? stdColor.lighter(50) : stdColor.darker(50);
    QList<int> sizes=QList<int>() << 16 << 22;

    foreach (int s, sizes) {
        icon.addPixmap(isSingle ? createSingleIconPixmap(s, stdColor, 100.0) : createConsumeIconPixmap(s, stdColor, 100.0));
        icon.addPixmap(isSingle ? createSingleIconPixmap(s, stdColor, 50.0) : createConsumeIconPixmap(s, stdColor, 50.0), QIcon::Disabled);
        icon.addPixmap(isSingle ? createSingleIconPixmap(s, highlightColor, 100.0) : createConsumeIconPixmap(s, highlightColor, 100.0), QIcon::Active);
    }

    return icon;
}

QIcon Icon::createSingleIcon()
{
    return createIcon(true);
}

QIcon Icon::createConsumeIcon()
{
    return createIcon(false);
}

int Icon::stdSize(int v)
{
    if (v<20) {
        return 16;
    } else if (v<28) {
        return 22;
    } else if (v<40) {
        return 32;
    } else if (v<56) {
        return 48;
    } else if (v<90) {
        return 64;
    } else {
        return 128;
    }
}

void Icon::init(QToolButton *btn, bool setFlat)
{
    static int size=-1;

    if (-1==size) {
        size=QApplication::fontMetrics().height();
        if (size>18) {
            size=stdSize(size*1.25);
        } else {
            size=16;
        }
    }
    btn->setIconSize(QSize(size, size));
    if (setFlat) {
        btn->setAutoRaise(true);
    }
}

Icn Icon::configureIcon;
Icn Icon::connectIcon;
Icn Icon::disconnectIcon;
Icn Icon::speakerIcon;
Icn Icon::lyricsIcon;
Icn Icon::dynamicIcon;
Icn Icon::playlistIcon;
Icn Icon::variousArtistsIcon;
Icn Icon::artistIcon;

void Icon::setupIconTheme()
{
    #ifdef Q_OS_WIN
    // Check that we have certain icons in the selected icon theme. If not, and oxygen is installed, then
    // set icon theme to oxygen.
    QString theme=QIcon::themeName();
    if (QLatin1String("oxygen")!=theme) {
        QStringList check=QStringList() << "actions/edit-clear-list" << "actions/view-media-playlist"
                                        << "actions/view-media-lyrics" << "actions/configure"
                                        << "actions/view-choose" << "actions/view-media-artist";
        bool found=false;

        foreach (const QString &icn, check) {
            if (!QIcon::hasThemeIcon(icn)) {
                QStringList paths=QIcon::themeSearchPaths();

                foreach (const QString &p, paths) {
                    if (QDir(p+QLatin1String("/oxygen")).exists()) {
                        QIcon::setThemeName(QLatin1String("oxygen"));
                        found=true;
                        break;
                    }
                }
                if (found) {
                    break;
                }
            }
        }
    }
    #endif

    configureIcon=Icon("configure");
    connectIcon=Icon("network-connect");
    disconnectIcon=Icon("network-disconnect");
    speakerIcon=Icon("speaker");
    lyricsIcon=Icon("view-media-lyrics");
    dynamicIcon=Icon("media-playlist-shuffle");
    playlistIcon=Icon("view-media-playlist");
    variousArtistsIcon=Icon("cantata-view-media-artist-various");
    artistIcon=Icon("view-media-artist");
    #if !defined ENABLE_KDE_SUPPORT && !defined Q_OS_WIN
    if (configureIcon.isNull()) {
        configureIcon=Icon("gtk-preferences");
    }
    if (connectIcon.isNull()) {
        connectIcon=Icon("connect_creating");
    }
    if (disconnectIcon.isNull()) {
        disconnectIcon=Icon("media-eject");
    }
    if (speakerIcon.isNull()) {
        speakerIcon=Icon("audio-speakers");
        if (speakerIcon.isNull()) {
            speakerIcon=Icon("gnome-volume-control");
        }
    }
    if (lyricsIcon.isNull()) {
        lyricsIcon=Icon("text-x-generic");
    }
    if (dynamicIcon.isNull()) {
        dynamicIcon=Icon("text-x-generic");
    }
    if (playlistIcon.isNull()) {
        playlistIcon=Icon("audio-x-mp3-playlist");
        if (playlistIcon.isNull()) {
            playlistIcon=Icon("audio-x-generic");
        }
    }
    if (variousArtistsIcon.isNull()) {
        variousArtistsIcon.addFile(":va16.png");
        variousArtistsIcon.addFile(":va22.png");
        variousArtistsIcon.addFile(":va32.png");
        variousArtistsIcon.addFile(":va48.png");
        variousArtistsIcon.addFile(":va64.png");
        variousArtistsIcon.addFile(":va128.png");
    }
    if (artistIcon.isNull()) {
        artistIcon.addFile(":artist16.png");
        artistIcon.addFile(":artist22.png");
        artistIcon.addFile(":artist32.png");
        artistIcon.addFile(":artist48.png");
    }
    #endif
}

#if !defined ENABLE_KDE_SUPPORT
QIcon Icon::getMediaIcon(const char *name)
{
    static QList<QIcon::Mode> modes=QList<QIcon::Mode>() << QIcon::Normal << QIcon::Disabled << QIcon::Active << QIcon::Selected;
    QIcon icn;
    QIcon icon=QIcon::fromTheme(name);

    foreach (QIcon::Mode mode, modes) {
        icn.addPixmap(icon.pixmap(QSize(64, 64), mode).scaled(QSize(28, 28), Qt::KeepAspectRatio, Qt::SmoothTransformation), mode);
        icn.addPixmap(icon.pixmap(QSize(22, 22), mode), mode);
    }

    return icn;
}

QIcon Icon::create(const QStringList &sizes)
{
    QIcon icon(sizes.at(0));
    for (int i=1; i<sizes.size(); ++i) {
        icon.addFile(sizes.at(i));
    }
    return icon;
}
#endif
