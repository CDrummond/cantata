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

#include "gtkstyle.h"
#include "config.h"
#include "utils.h"
#include <QPainter>
#include <QApplication>
#include <QCache>
#include <QProcess>
#include <QTextStream>
#include <QFile>
#include <qglobal.h>
#include "touchproxystyle.h"

#if defined Q_OS_WIN || defined Q_OS_MAC || defined QT_NO_STYLE_GTK
#define NO_GTK_SUPPORT
#endif

#ifndef NO_GTK_SUPPORT
#include "gtkproxystyle.h"
#endif

static bool usingGtkStyle=false;

bool GtkStyle::isActive()
{
    #ifndef NO_GTK_SUPPORT
    static bool init=false;
    if (!init) {
        init=true;
        usingGtkStyle=QApplication::style()->inherits("QGtkStyle");
        if (usingGtkStyle) {
            QCoreApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
        }
    }
    #endif
    return usingGtkStyle;
}

void GtkStyle::drawSelection(const QStyleOptionViewItem &opt, QPainter *painter, double opacity)
{
    static const int constMaxDimension=32;
    static QCache<QString, QPixmap> cache(30000);

    if (opt.rect.width()<2 || opt.rect.height()<2) {
        return;
    }

    int width=qMin(constMaxDimension, opt.rect.width());
    QString key=QString::number(width)+QChar(':')+QString::number(opt.rect.height());
    QPixmap *pix=cache.object(key);

    if (!pix) {
        pix=new QPixmap(width, opt.rect.height());
        QStyleOptionViewItem styleOpt(opt);
        pix->fill(Qt::transparent);
        QPainter p(pix);
        styleOpt.state=opt.state;
        styleOpt.state&=~(QStyle::State_Selected|QStyle::State_MouseOver);
        styleOpt.state|=QStyle::State_Selected|QStyle::State_Enabled|QStyle::State_Active;
        styleOpt.viewItemPosition = QStyleOptionViewItem::OnlyOne;
        styleOpt.rect=QRect(0, 0, opt.rect.width(), opt.rect.height());
        QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &styleOpt, &p, 0);
        p.end();
        cache.insert(key, pix, pix->width()*pix->height());
    }
    double opacityB4=painter->opacity();
    painter->setOpacity(opacity);
    if (opt.rect.width()>pix->width()) {
        int half=qMin(opt.rect.width()>>1, pix->width()>>1);
        painter->drawPixmap(opt.rect.x(), opt.rect.y(), pix->copy(0, 0, half, pix->height()));
        if ((half*2)!=opt.rect.width()) {
            painter->drawTiledPixmap(opt.rect.x()+half, opt.rect.y(), (opt.rect.width()-((2*half))), opt.rect.height(), pix->copy(half-1, 0, 1, pix->height()));
        }
        painter->drawPixmap((opt.rect.x()+opt.rect.width())-half, opt.rect.y(), pix->copy(half, 0, half, pix->height()));
    } else {
        painter->drawPixmap(opt.rect, *pix);
    }
    painter->setOpacity(opacityB4);
}

static QProxyStyle *proxyStyle=0;

void GtkStyle::applyTheme(QWidget *widget)
{
    #ifdef NO_GTK_SUPPORT
    Q_UNUSED(widget)
    #else
    if (widget && isActive() && !proxyStyle) {
        proxyStyle=new GtkProxyStyle(0);
    }
    #endif

    #if defined Q_OS_WIN
    int modViewFrame=ProxyStyle::VF_Side;
    #elif defined Q_OS_MAC
    int modViewFrame=ProxyStyle::VF_Side|ProxyStyle::VF_Top;
    #else
    int modViewFrame=0;
    #endif

    if (!proxyStyle && Utils::touchFriendly()) {
        proxyStyle=new TouchProxyStyle(modViewFrame);
    }
    if (!proxyStyle && modViewFrame) {
        proxyStyle=new ProxyStyle(modViewFrame);
    }
    if (proxyStyle) {
        qApp->setStyle(proxyStyle);
    }
}

