/*
 * Cantata
 *
 * Copyright (c) 2011-2017 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "monoicon.h"
#include "utils.h"
#ifdef Q_OS_MAC
#include "osxstyle.h"
#endif
#include <QFile>
#include <QIconEngine>
#include <QSvgRenderer>
#include <QPainter>
#include <QPixmapCache>
#include <QRect>
#include <QFontDatabase>
#include <QApplication>
#include <QPalette>

class MonoIconEngine : public QIconEngine
{
public:
    MonoIconEngine(const QString &file, FontAwesome::icon fa, const QColor &col, const QColor &sel)
        : fileName(file)
        , fontAwesomeIcon(fa)
        , color(col)
        , selectedColor(sel)
    {
    }

    virtual ~MonoIconEngine() {}

    MonoIconEngine * clone() const
    {
        return new MonoIconEngine(fileName, fontAwesomeIcon, color, selectedColor);
    }

    virtual void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state)
    {
        Q_UNUSED(state)

        QColor col=QIcon::Selected==mode ? selectedColor : color;
        if (QIcon::Selected==mode && !col.isValid()) {
            #ifdef Q_OS_MAC
            col=OSXStyle::self()->viewPalette().highlightedText().color();
            #else
            col=QApplication::palette().highlightedText().color();
            #endif
        }
        QString key=(fileName.isEmpty() ? QString::number(fontAwesomeIcon) : fileName)+
                    QLatin1Char('-')+QString::number(rect.width())+QLatin1Char('-')+QString::number(rect.height())+QLatin1Char('-')+col.name();
        QPixmap pix;

        if (!QPixmapCache::find(key, &pix)) {
            pix=QPixmap(rect.width(), rect.height());
            pix.fill(Qt::transparent);
            QPainter p(&pix);

            if (fileName.isEmpty()) {
                QString fontName;
                if (FontAwesome::ex_one==fontAwesomeIcon) {
                    fontName="serif";
                } else {
                    // Load fontawesome, if it is not already loaded
                    if (fontAwesomeFontName.isEmpty()) {
                        QStringList loadedFontFamilies = QFontDatabase::applicationFontFamilies(QFontDatabase::addApplicationFont(Utils::systemDir(QLatin1String("fonts"))+QLatin1String("fontawesome-webfont.ttf")));
                        if (!loadedFontFamilies.empty()) {
                            fontAwesomeFontName= loadedFontFamilies.at(0);
                        }
                    }
                    fontName=fontAwesomeFontName;
                }

                QFont font(fontName);
                int pixelSize=rect.height();
                if (FontAwesome::ex_one==fontAwesomeIcon) {
                    font.setBold(true);
                } else if (pixelSize>10) {
                    static const int constScale=14;
                    static const int constHalfScale=constScale/2;
                    pixelSize=((pixelSize/constScale)*constScale)+((pixelSize%constScale)>=constHalfScale ? constScale : 0);
                    pixelSize=qMin(pixelSize, rect.height());
                    if (FontAwesome::bars==fontAwesomeIcon && pixelSize%constHalfScale) {
                        pixelSize-=1;
                    } else if (FontAwesome::list==fontAwesomeIcon && pixelSize%constHalfScale) {
                        pixelSize+=1;
                    }

                }

                font.setPixelSize(pixelSize);
                font.setStyleStrategy(QFont::PreferAntialias);
                font.setHintingPreference(QFont::PreferFullHinting);
                p.setFont(font);
                p.setPen(col);
                p.setRenderHint(QPainter::Antialiasing, true);
                if (FontAwesome::ex_one==fontAwesomeIcon) {
                    QString str=QString::number(fontAwesomeIcon);
                    p.drawText(QRect(0, 0, rect.width(), rect.height()), str, QTextOption(Qt::AlignHCenter|Qt::AlignVCenter));
                    p.drawText(QRect(1, 0, rect.width(), rect.height()), str, QTextOption(Qt::AlignHCenter|Qt::AlignVCenter));
                } else {
                    p.drawText(QRect(0, 0, rect.width(), rect.height()), QString(QChar(static_cast<int>(fontAwesomeIcon))), QTextOption(Qt::AlignCenter|Qt::AlignVCenter));
                }
            } else {
                QSvgRenderer renderer;
                QFile f(fileName);
                QByteArray bytes;
                if (f.open(QIODevice::ReadOnly)) {
                    bytes=f.readAll();
                }
                if (!bytes.isEmpty()) {
                    bytes.replace("#000", col.name().toLatin1());
                }
                renderer.load(bytes);
                renderer.render(&p, QRect(0, 0, rect.width(), rect.height()));
            }
            QPixmapCache::insert(key, pix);
        }
        if (QIcon::Disabled==mode) {
            painter->save();
            painter->setOpacity(painter->opacity()*0.35);
        }
        painter->drawPixmap(rect.topLeft(), pix);
        if (QIcon::Disabled==mode) {
            painter->restore();
        }
    }

    virtual QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state)
    {
        QPixmap pix(size);
        pix.fill(Qt::transparent);
        QPainter painter(&pix);
        paint(&painter, QRect(QPoint(0, 0), size), mode, state);
        return pix;
    }

private:
    QString fileName;
    FontAwesome::icon fontAwesomeIcon;
    QColor color;
    QColor selectedColor;
    static QString fontAwesomeFontName;
};

QString MonoIconEngine::fontAwesomeFontName;

const QColor MonoIcon::constRed(220, 0, 0);

QIcon MonoIcon::icon(const QString &fileName, const QColor &col, const QColor &sel)
{
    return QIcon(new MonoIconEngine(fileName, (FontAwesome::icon)0, col, sel));
}

QIcon MonoIcon::icon(const FontAwesome::icon icon, const QColor &col, const QColor &sel)
{
    return QIcon(new MonoIconEngine(QString(), icon, col, sel));
}
