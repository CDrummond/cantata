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

#include "textbrowser.h"
#include <QImage>
#include <QScrollBar>
#include <QEvent>
#include <QStyle>
#include <QTimer>

TextBrowser::TextBrowser(QWidget *p)
    : QTextBrowser(p)
    , timer(nullptr)
    , lastImageSize(0)
    , fullWidthImg(false)
    , haveImg(false)
{
}

// QTextEdit/QTextBrowser seems to do FastTransformation when scaling images, and this looks bad.
QVariant TextBrowser::loadResource(int type, const QUrl &name)
{
    if (QTextDocument::ImageResource==type) {
        if ((name.scheme().isEmpty() || QLatin1String("file")==name.scheme())) {
            QImage img;
            img.load(name.path());
            if (!img.isNull()) {
                haveImg=true;
                return img.scaled(imageSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
        } else if (QLatin1String("data")==name.scheme()) {
            QByteArray encoded=name.toEncoded();
            static const QString constStart("data:image/png;base64,");
            encoded=QByteArray::fromBase64(encoded.mid(constStart.length()));
            QImage img;
            img.loadFromData(encoded);
            if (!img.isNull()) {
                haveImg=true;
                return img.scaled(imageSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
        }
    }
    return QTextBrowser::loadResource(type, name);
}

void TextBrowser::setFullWidthImage(bool s)
{
    if (s!=fullWidthImg) {
        fullWidthImg=s;
        verticalScrollBar()->removeEventFilter(this);
        if (fullWidthImg) {
            verticalScrollBar()->installEventFilter(this);
        }
        if (haveImg) {
            setHtml(toHtml());
        }
    }
}

void TextBrowser::setPal(const QPalette &pal)
{
    setPalette(pal);
    verticalScrollBar()->setPalette(pal);
    horizontalScrollBar()->setPalette(pal);
}

void TextBrowser::resizeEvent(QResizeEvent *e)
{
    handleSizeChange();
    QTextBrowser::resizeEvent(e);
}

bool TextBrowser::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj==verticalScrollBar() && (QEvent::Show==ev->type() || QEvent::Hide==ev->type())) {
        handleSizeChange();
    }
    return QTextBrowser::eventFilter(obj, ev);
}

void TextBrowser::refreshHtml()
{
    setHtml(toHtml());
}

void TextBrowser::handleSizeChange()
{
    if (haveImg) {
        int imgSize=imageSize().width();
        if (imgSize!=lastImageSize) {
            if (timer) {
                timer->stop();
            } else {
                timer=new QTimer(this);
                timer->setSingleShot(true);
                connect(timer, SIGNAL(timeout()), SLOT(refreshHtml()));
            }
            lastImageSize=imgSize;
            timer->start();
        }
    }
}

QSize TextBrowser::imageSize() const
{
    QSize sz;

    int sbarSpacing = style()->pixelMetric(QStyle::PM_ScrollView_ScrollBarSpacing);
    if (sbarSpacing<0) {
        sz=size()-QSize(4, 0);
    } else {
        QScrollBar *sb=verticalScrollBar();
        int sbarWidth=sb && sb->isVisible() ? 0 : style()->pixelMetric(QStyle::PM_ScrollBarExtent);
        sz=size()-QSize(4 + sbarSpacing + sbarWidth, 0);
    }
    if (fullWidthImg || sz.width()<=picSize().width()) {
        return sz.width()<32 || sz.height()<32 ? QSize(32, 32) : sz;
    }
    return picSize();
}

#include "moc_textbrowser.cpp"
