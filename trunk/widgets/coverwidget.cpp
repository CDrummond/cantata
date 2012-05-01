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

#include "coverwidget.h"
#include "covers.h"
#include "config.h"
#include <QtGui/QPixmap>
#include <QtGui/QIcon>
#include <QtCore/QEvent>
#include <QtCore/QTimer>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocale>
#include <KDE/KTemporaryFile>
#else
#include <QtCore/QTemporaryFile>
#endif

CoverWidget::CoverWidget(QWidget *parent)
    : QLabel(parent)
    , empty(true)
    , valid(false)
    , tempFile(0)
{
    connect(Covers::self(), SIGNAL(cover(const Song &, const QImage &, const QString &)), SLOT(coverRetreived(const Song &, const QImage &, const QString &)));
    update(noCover);
    installEventFilter(this);
    QTimer::singleShot(0, this, SLOT(init())); // Need to do this after constructed, so that size is set....
}

CoverWidget::~CoverWidget()
{
    delete tempFile;
}

const QPixmap & CoverWidget::stdPixmap(bool stream)
{
    QPixmap &pix=stream ? noStreamCover : noCover;

    if (pix.isNull()) {
        pix = QIcon::fromTheme(stream ? DEFAULT_STREAM_ICON : DEFAULT_ALBUM_ICON).pixmap(128, 128).scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return pix;
}

void CoverWidget::update(const QImage &img)
{
    setPixmap(QPixmap::fromImage(img).scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    image=img;
    empty=false;
}

void CoverWidget::update(const QPixmap &pix)
{
    setPixmap(pix);
    image=QImage();
    empty=true;
}

void CoverWidget::update(const Song &s)
{
    if (s.albumArtist()!= current.albumArtist() || s.album != current.album) {
        current=s;
        if (!s.albumArtist().isEmpty() && !s.album.isEmpty()) {
            Covers::Image img=Covers::self()->get(s);
            valid=!img.img.isNull();
            if (valid) {
                update(img.img);
                coverFileName=img.fileName;
                emit coverImage(img.img);
                emit coverFile(img.fileName);
            }
        } else {
            valid=false;
            coverFileName=QString();
            update(stdPixmap(current.isStream()));
            emit coverImage(QImage());
            emit coverFile(QString());;
        }
    }
}

void CoverWidget::init()
{
    update(stdPixmap(false));
}

void CoverWidget::coverRetreived(const Song &s, const QImage &img, const QString &file)
{
    if (s.year==current.year && s.albumArtist()==current.albumArtist() && s.album==current.album) {
        valid=!img.isNull();
        if (valid) {
            update(img);
            coverFileName=file;
            emit coverImage(img);
            emit coverFile(file);
        } else {
            update(stdPixmap(current.isStream()));
            coverFileName=QString();
            emit coverImage(QImage());
            emit coverFile(QString());
        }
    }
}

bool CoverWidget::eventFilter(QObject *object, QEvent *event)
{
    if (event->type()==QEvent::ToolTip) {
        QString toolTip=QLatin1String("<table>");

        #ifdef ENABLE_KDE_SUPPORT
        toolTip+=i18n("<tr><td align=\"right\"><b>Artist:</b></td><td>%1</td></tr>"
                      "<tr><td align=\"right\"><b>Album:</b></td><td>%2</td></tr>"
                      "<tr><td align=\"right\"><b>Year:</b></td><td>%3</td></tr>").arg(current.artist).arg(current.album).arg(current.year);
        #else
        toolTip+=tr("<tr><td align=\"right\"><b>Artist:</b></td><td>%1</td></tr>"
                    "<tr><td align=\"right\"><b>Album:</b></td><td>%2</td></tr>"
                    "<tr><td align=\"right\"><b>Year:</b></td><td>%3</td></tr>").arg(current.artist).arg(current.album).arg(current.year);
        #endif
        toolTip+="</table>";
        if (!coverFileName.isEmpty() && !image.isNull()) {
            if (image.size().width()>Covers::constMaxSize.width() || image.size().height()>Covers::constMaxSize.height()) {
                if (!tempFile) {
                    #ifdef ENABLE_KDE_SUPPORT
                    tempFile=new KTemporaryFile();
                    tempFile->setSuffix(".jpg");
                    #else
                    tempFile=new QTemporaryFile("/tmp/XXXXXX.jpg");
                    #endif
                    tempFile->setAutoRemove(true);
                    tempFile->open();
                }
                QString tempCover=tempFile->property("orig").toString();
                if (tempCover!=coverFileName) {
                    QImage scaled=image.scaled(Covers::constMaxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    scaled.save(tempFile->fileName());
                    tempFile->setProperty("orig", coverFileName);
                }
                toolTip+=QString("<br/><img src=\"%1\"/>").arg(tempFile->fileName());
            } else {
                toolTip+=QString("<br/><img src=\"%1\"/>").arg(coverFileName);
            }
        }
        setToolTip(toolTip);
    }
    return QObject::eventFilter(object, event);
}
