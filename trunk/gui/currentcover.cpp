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

#include "currentcover.h"
#include "covers.h"
#include "config.h"
#include "widgets/icons.h"
#include "support/utils.h"
#include "support/globalstatic.h"
#include <QFile>

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
    foreach (const QString &p, paths) {
        QString index(p+"/"+theme+"/index.theme");
        QFile f(index);
        if (f.open(QIODevice::ReadOnly|QIODevice::Text)) {
            while (!f.atEnd()) {
                QString line=QString::fromUtf8(f.readLine()).trimmed().simplified();
                if (line.startsWith(key)) {
                    QStringList inherited=line.mid(key.length()).split(",", QString::SkipEmptyParts);
                    foreach (const QString &i, inherited) {
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
        themes(Icon::currentTheme(), iconThemes);
    }
}

QString CurrentCover::findIcon(const QStringList &names)
{
    initIconThemes();
    QList<int> sizes=QList<int>() << 256 << 128 << 64 << 48 << 32 << 24 << 22;
    QStringList paths=QIcon::themeSearchPaths();
    QStringList sections=QStringList() << "categories" << "devices";
    foreach (const QString &p, paths) {;
        foreach (const QString &n, names) {
            foreach (const QString &theme, iconThemes) {
                QMap<int, QString> files;
                foreach (int s, sizes) {
                    foreach (const QString &sect, sections) {
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
                    foreach (int s, sizes) {
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
    : QObject(0)
    , enabled(false)
    , empty(true)
    , valid(false)
{
}

CurrentCover::~CurrentCover()
{
}

const QImage & CurrentCover::stdImage(bool stream)
{
    QImage &img=stream ? noStreamCover : noCover;

    #ifdef ENABLE_UBUNTU
    // TODO: Just return qrc file Touch...
    #else
    if (img.isNull()) {
        int iconSize=Icon::stdSize(Utils::scaleForDpi(128));
        img = (stream ? Icons::self()->streamIcon : Icons::self()->albumIcon).pixmap(iconSize, iconSize).toImage();

        QString &file=stream ? noStreamCoverFileName : noCoverFileName;
        if (stream && file.isEmpty()) {
            QString iconFile=QString(CANTATA_SYS_ICONS_DIR+"stream.png");
            if (QFile::exists(iconFile)) {
                file=iconFile;
            }
        }
        #if !defined Q_OS_WIN && !defined Q_OS_MAC
        if (file.isEmpty()) {
            file=findIcon(stream ? QStringList() << "applications-internet" : QStringList() << "media-optical" << "media-optical-audio");
        }
        #endif
    }
    #endif
    return img;
}

void CurrentCover::setEnabled(bool e)
{
    if (enabled==e) {
        return;
    }

    enabled=e;
    if (enabled) {
        img=stdImage(false);
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

    if (s.albumArtist()!=current.albumArtist() || s.album!=current.album || s.isStream()!=current.isStream()) {
        current=s;
        if (!s.albumArtist().isEmpty() && !s.album.isEmpty() && !current.isStandardStream()) {
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
                img=stdImage(false);
            }
        } else {
            valid=true;
            img=stdImage(current.isStandardStream());
            coverFileName=current.isStandardStream() ? noStreamCoverFileName : noCoverFileName;
            emit coverFile(coverFileName);
            emit coverImage(QImage());
        }
    }
}

void CurrentCover::coverRetrieved(const Song &s, const QImage &img, const QString &file)
{
    if (!s.isArtistImageRequest() && s.albumArtist()==current.albumArtist() && s.album==current.album) {
        valid=!img.isNull();
        if (valid) {
            coverFileName=file;
            emit coverFile(file);
            emit coverImage(img);
        } else {
            coverFileName=current.isStandardStream() ? noStreamCoverFileName : noCoverFileName;
            emit coverFile(coverFileName);
            emit coverImage(QImage());
        }
    }
}
