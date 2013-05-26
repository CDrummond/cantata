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

#include "textbrowser.h"
#include "spinner.h"
#include <QPainter>
#include <QDebug>

static int border=-1;
static int minSize=-1;
static int maxSize=-1;

static int toGray(int r, int g, int b)
{
    return (r+g+b)/3; // QColor(r, g, b).value();
}

static void toGray(QImage &img)
{
    unsigned char *data=img.bits();
    int width=img.width()*4;
    int height=img.height();

    for(int row=0; row<height; ++row) {
        int offset=row*img.bytesPerLine();
        for(int column=0; column<width; column+=4) {
            #if Q_BYTE_ORDER == Q_BIG_ENDIAN
            // ARGB
            int gray=toGray(data[offset+column+1], data[offset+column+2], data[offset+column+3]);
            data[offset+column+1] = gray;
            data[offset+column+2] = gray;
            data[offset+column+3] = gray;
            #else
            // BGRA
            int gray=toGray(data[offset+column+2], data[offset+column+1], data[offset+column]);
            data[offset+column] = gray;
            data[offset+column+1] = gray;
            data[offset+column+2] = gray;
            #endif
        }
    }
}

TextBrowser::TextBrowser(QWidget *p)
    : QTextBrowser(p)
    , drawImage(false)
    , spinner(0)
{
    orig=font().pointSize();
    if (-1==minSize) {
        minSize=fontMetrics().height()*18;
        minSize=(((int)(minSize/100))*100)+(minSize%100 ? 100 : 0);
        maxSize=minSize*2;
        border=minSize/16;
    }
    setReadOnly(true);
}

void TextBrowser::setReadOnly(bool ro)
{
    viewport()->update();
    QTextBrowser::setReadOnly(ro);
    viewport()->update();
}

void TextBrowser::setImage(const QImage &img)
{
    if (drawImage && (!img.isNull() || (img.isNull()!=image.isNull()))) {
        image=img;
        if (!image.isNull()) {
            if (image.width()<minSize || image.height()<minSize) {
                image=image.scaled(QSize(minSize, minSize), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            } else if (image.width()>maxSize || image.height()>maxSize) {
                image=image.scaled(QSize(maxSize, maxSize), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }

            image=image.convertToFormat(QImage::Format_ARGB32);
            toGray(image);
        }
        viewport()->update();
    }
}

void TextBrowser::enableImage(bool e)
{
    if (e!=drawImage) {
        drawImage=e;
        if (!drawImage) {
            image=QImage();
        }
        viewport()->setAutoFillBackground(!e);
        viewport()->update();
    }
}

void TextBrowser::paintEvent(QPaintEvent *e)
{
    if (drawImage && isReadOnly() && !image.isNull()) {
        QPainter p(viewport());
        p.setOpacity(0.15);
//        QRect r(rect());
//        int xpos=r.x()+(r.width()>image.width() ? ((r.width()-image.width())/2) : 0);
//        int ypos=r.y()+(r.height()>image.height() ? ((r.height()-image.height())/2) : 0);
//        p.drawImage(xpos, ypos, image);
        p.fillRect(rect(), QBrush(image));
    } else if (!isReadOnly() && !viewport()->autoFillBackground()) {
        QPainter p(viewport());
        p.fillRect(rect(), viewport()->palette().base());
    }

    QTextBrowser::paintEvent(e);
}

void TextBrowser::setHtml(const QString &txt, bool showSpin)
{
    QTextBrowser::setHtml(txt);
    if (showSpin) {
        showSpiner();
    } else {
        hideSpinner();
    }
}

void TextBrowser::setText(const QString &txt, bool showSpin)
{
    QTextBrowser::setText(txt);
    if (showSpin) {
        showSpiner();
    } else {
        hideSpinner();
    }
}

void TextBrowser::showSpiner()
{
    if (!spinner) {
        spinner=new Spinner(this);
        spinner->setWidget(this);
    }
    if (!spinner->isActive()) {
        spinner->start();
    }
}

void TextBrowser::hideSpinner()
{
    if (spinner) {
        spinner->stop();
    }
}
