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
#include "localize.h"
#include "icon.h"
#include <QtGui/QPixmap>
#include <QtCore/QBuffer>
#include <QtCore/QEvent>
#include <QtCore/QFile>
#include <QtCore/QTimer>
#include <QtCore/QVariant>

static QString encode(const QImage &img)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    QImage copy(img);

    if (copy.size().width()>Covers::constMaxSize.width() || copy.size().height()>Covers::constMaxSize.height()) {
        copy=copy.scaled(Covers::constMaxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    copy.save(&buffer, "PNG");
    return QString("<img src=\"data:image/png;base64,%1\">").arg(QString(buffer.data().toBase64()));
}

CoverWidget::CoverWidget(QWidget *parent)
    : QLabel(parent)
    , empty(true)
    , valid(false)
{
    connect(Covers::self(), SIGNAL(cover(const Song &, const QImage &, const QString &)), SLOT(coverRetreived(const Song &, const QImage &, const QString &)));
    update(noCover);
    installEventFilter(this);
    QTimer::singleShot(0, this, SLOT(init())); // Need to do this after constructed, so that size is set....
}

CoverWidget::~CoverWidget()
{
}

const QPixmap & CoverWidget::stdPixmap(bool stream)
{
    QPixmap &pix=stream ? noStreamCover : noCover;

    if (pix.isNull()) {
        pix = (stream ? Icon::streamIcon : Icon::albumIcon).pixmap(128, 128).scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return pix;
}

void CoverWidget::update(const QImage &i)
{
    setPixmap(QPixmap::fromImage(i).scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    img=i;
    empty=false;
}

void CoverWidget::update(const QPixmap &pix)
{
    setPixmap(pix);
    img=QImage();
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
            emit coverFile(QString());
        }
    }
}

void CoverWidget::init()
{
    update(stdPixmap(false));
}

void CoverWidget::coverRetreived(const Song &s, const QImage &img, const QString &file)
{
    if (s.albumArtist()==current.albumArtist() && s.album==current.album) {
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

        toolTip+=i18n("<tr><td align=\"right\"><b>Artist:</b></td><td>%1</td></tr>"
                      "<tr><td align=\"right\"><b>Album:</b></td><td>%2</td></tr>"
                      "<tr><td align=\"right\"><b>Year:</b></td><td>%3</td></tr>").arg(current.artist).arg(current.album).arg(current.year);
        toolTip+="</table>";
        if (!img.isNull()) {
            if (img.size().width()>Covers::constMaxSize.width() || img.size().height()>Covers::constMaxSize.height() ||
                coverFileName.isEmpty() || !QFile::exists(coverFileName)) {
                toolTip+=QString("<br/>%1").arg(encode(img));
            } else {
                toolTip+=QString("<br/><img src=\"%1\"/>").arg(coverFileName);
            }
        }
        setToolTip(toolTip);
    }
    return QObject::eventFilter(object, event);
}

const QImage & CoverWidget::image() const
{
    if (img.isNull()) {
        const QPixmap *pix = pixmap();
        if (pix && !pix->isNull()) {
            img=pix->toImage();
        }
    }
    return img;
}

