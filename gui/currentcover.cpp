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

#include "currentcover.h"
#include "covers.h"
#include "config.h"
#include "widgets/icons.h"
#include "support/utils.h"
#include "support/globalstatic.h"
#include "online/onlineservice.h"
#include <QFile>
#include <QTimer>

#if !defined Q_OS_WIN && !defined Q_OS_MAC
static void themes(const QString &theme, QStringList &iconThemes)
{
    if (iconThemes.contains(theme)) {
        return;
    }
    QString lower=theme.toLower();
    iconThemes << theme;
    if (lower!=theme) {
        iconThemes << lower;
    }
    QStringList paths=QIcon::themeSearchPaths();
    QString key("Inherits=");
    for (const QString &p: paths) {
        QString index(p+"/"+theme+"/index.theme");
        QFile f(index);
        if (f.open(QIODevice::ReadOnly|QIODevice::Text)) {
            while (!f.atEnd()) {
                QString line=QString::fromUtf8(f.readLine()).trimmed().simplified();
                if (line.startsWith(key)) {
                    QStringList inherited=line.mid(key.length()).split(",", QString::SkipEmptyParts);
                    for (const QString &i: inherited) {
                        themes(i, iconThemes);
                    }
                    return;
                }
            }
        }
    }
}

void CurrentCover::initIconThemes()
{
    if (iconThemes.isEmpty()) {
        themes(QIcon::themeName(), iconThemes);
    }
}

QString CurrentCover::findIcon(const QStringList &names)
{
    initIconThemes();
    QList<int> sizes=QList<int>() << 256 << 128 << 64 << 48 << 32 << 24 << 22;
    QStringList paths=QIcon::themeSearchPaths();
    QStringList sections=QStringList() << "apps" << "categories" << "devices" << "mimetypes" << "mimes";
    for (const QString &theme: iconThemes) {
        for (const QString &n: names) {
            for (const QString &p: paths) {
                QMap<int, QString> files;
                for (int s: sizes) {
                    for (const QString &sect: sections) {
                        QString f(p+"/"+theme+"/"+QString::number(s)+"x"+QString::number(s)+"/"+sect+"/"+n+".png");
                        if (QFile::exists(f)) {
                            files.insert(s, f);
                            break;
                        }
                        f=QString(p+"/"+theme+"/"+QString::number(s)+"x"+QString::number(s)+"/"+sect+"/"+n+".svg");
                        if (QFile::exists(f)) {
                            files.insert(s, f);
                            break;
                        }
                        f=QString(p+"/"+theme+"/"+sect+"/"+QString::number(s)+"/"+n+".svg");
                        if (QFile::exists(f)) {
                            files.insert(s, f);
                            break;
                        }
                    }
                }

                if (!files.isEmpty()) {
                    for (int s: sizes) {
                        if (files.contains(s)) {
                            return files[s];
                        }
                    }
                }
            }
        }
    }
    return QString();
}
#endif

GLOBAL_STATIC(CurrentCover, instance)

CurrentCover::CurrentCover()
    : QObject(nullptr)
    , enabled(false)
    , valid(false)
    , timer(nullptr)
{
    img=stdImage(current);
    coverFileName=noCoverFileName;
}

CurrentCover::~CurrentCover()
{
}

const QImage & CurrentCover::stdImage(const Song &s)
{
    bool podcast=s.isFromOnlineService() && OnlineService::isPodcasts(s.onlineService());
    bool stream=s.isStandardStream() || OnlineService::showLogoAsCover(s);

    QImage &img=podcast
                ? noPodcastCover
                : stream
                    ? noStreamCover
                    : noCover;

    if (img.isNull()) {
        int iconSize=Icon::stdSize(Utils::scaleForDpi(128));
        const QIcon &icon=podcast
                ? Icons::self()->podcastIcon
                : stream
                    ? Icons::self()->streamIcon
                    : Icons::self()->albumIcon(iconSize);
        img=icon.pixmap(iconSize, iconSize).toImage();

        QString &file=podcast
                      ? noPodcastCoverFileName
                      : stream
                          ? noStreamCoverFileName
                          : noCoverFileName;

        if (file.isEmpty()) {
            if (podcast) {
                QString iconFile=QString(CANTATA_SYS_ICONS_DIR+"podcasts.png");
                if (QFile::exists(iconFile)) {
                    file=iconFile;
                }
            } else if (stream) {
                QString iconFile=QString(CANTATA_SYS_ICONS_DIR+"stream.png");
                if (QFile::exists(iconFile)) {
                    file=iconFile;
                }
            }
        }
        #if !defined Q_OS_WIN && !defined Q_OS_MAC
        if (file.isEmpty()) {
            QStringList iconNames=podcast
                    ? QStringList{ QStringLiteral("x-media-podcast"), QStringLiteral("news-feed"), QStringLiteral("application-rss+xml"), QStringLiteral("application-atom+xml") }
                    : stream
                        ? QStringList{ QStringLiteral("applications-internet") }
                        : QStringList{ QStringLiteral("media-optical"), QStringLiteral("media-optical-audio") };
            file=findIcon(iconNames);
        }
        #endif
    }
    return img;
}

void CurrentCover::setEnabled(bool e)
{
    if (enabled==e) {
        return;
    }

    enabled=e;
    if (enabled) {
        img=stdImage(Song());
        connect(Covers::self(), SIGNAL(cover(const Song &, const QImage &, const QString &)), this, SLOT(coverRetrieved(const Song &, const QImage &, const QString &)));
        connect(Covers::self(), SIGNAL(coverUpdated(const Song &, const QImage &, const QString &)), this, SLOT(coverRetrieved(const Song &, const QImage &, const QString &)));
    } else {
        disconnect(Covers::self(), SIGNAL(cover(const Song &, const QImage &, const QString &)), this, SLOT(coverRetrieved(const Song &, const QImage &, const QString &)));
        disconnect(Covers::self(), SIGNAL(coverUpdated(const Song &, const QImage &, const QString &)), this, SLOT(coverRetrieved(const Song &, const QImage &, const QString &)));
        current=Song();
    }
}

void CurrentCover::update(const Song &s)
{
    if (!enabled) {
        return;
    }

    if (s.albumArtist()!=current.albumArtist() || s.album!=current.album || s.isStream()!=current.isStream() ||
        s.onlineService()!=current.onlineService()) {
        current=s;
        if (timer) {
            timer->stop();
        }
        if (!s.isUnknownAlbum() && ((!s.albumArtist().isEmpty() && !s.album.isEmpty() && !current.isStandardStream()) || OnlineService::showLogoAsCover(s))) {
            Covers::Image cImg=Covers::self()->requestImage(s, true);
            valid=!cImg.img.isNull();
            if (valid) {
                coverFileName=cImg.fileName;
                img=cImg.img;
                emit coverFile(cImg.fileName);
                if (current.isFromOnlineService()) {
                    if (coverFileName.startsWith(CANTATA_SYS_ICONS_DIR)) {
                        emit coverImage(QImage());
                    } else {
                        emit coverImage(cImg.img);
                    }
                } else {
                    emit coverImage(cImg.img);
                }
            } else {
                // We need to set the image here, so that TrayItem gets the correct 'noCover' image
                // ...but if Covers does eventually download a cover, we dont want valid->noCover->valid
                img=stdImage(current);
                // Start a timer - in case cover retrieval fails...
                if (!timer) {
                    timer=new QTimer(this);
                    timer->setSingleShot(true);
                    connect(timer, SIGNAL(timeout()), this, SLOT(setDefault()));
                }
                timer->start(750);
            }
        } else {
            valid=true;
            img=stdImage(current);
            coverFileName=current.isStandardStream() ? noStreamCoverFileName : noCoverFileName;
            emit coverFile(coverFileName);
            emit coverImage(QImage());
        }
    }
}

void CurrentCover::coverRetrieved(const Song &s, const QImage &newImage, const QString &file)
{
    if (!s.isArtistImageRequest() && s.albumArtist()==current.albumArtist() && s.album==current.album) {
        if (timer) {
            timer->stop();
        }
        valid=!newImage.isNull();
        if (valid) {
            coverFileName=file;
            img=newImage;
            emit coverFile(file);
            emit coverImage(newImage);
        } else {
            coverFileName=current.isStandardStream() ? noStreamCoverFileName : noCoverFileName;
            emit coverFile(coverFileName);
            emit coverImage(QImage());
        }
    }
}

void CurrentCover::setDefault()
{
    coverFileName=current.isStandardStream() ? noStreamCoverFileName : noCoverFileName;
    emit coverFile(coverFileName);
    emit coverImage(QImage());
}

#include "moc_currentcover.cpp"
