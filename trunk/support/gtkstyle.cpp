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

#include "gtkstyle.h"
#include "config.h"
#include <QPainter>
#include <QStyleOptionViewItemV4>
#include <QApplication>
#include <QCache>
#include <QProcess>
#include <QTextStream>
#include <QFile>
#include <qglobal.h>
#ifndef Q_OS_WIN
#include "gtkproxystyle.h"
#include "windowmanager.h"
#if QT_VERSION < 0x050000
#include <QGtkStyle>
#endif
#endif

static bool usingGtkStyle=false;
static bool useFullGtkStyle=false;

static inline void setup()
{
    #if !defined Q_OS_WIN
    static bool init=false;
    if (!init) {
        init=true;
        usingGtkStyle=QApplication::style()->inherits("QGtkStyle");
        useFullGtkStyle=usingGtkStyle && qgetenv("KDE_FULL_SESSION").isEmpty();
    }
    #endif
}

bool GtkStyle::isActive()
{
    setup();
    return usingGtkStyle;
}

bool GtkStyle::mimicWidgets()
{
    setup();
    return useFullGtkStyle;
}

void GtkStyle::drawSelection(const QStyleOptionViewItemV4 &opt, QPainter *painter, double opacity)
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
        QStyleOptionViewItemV4 styleOpt(opt);
        pix->fill(Qt::transparent);
        QPainter p(pix);
        styleOpt.state=opt.state;
        styleOpt.state&=~(QStyle::State_Selected|QStyle::State_MouseOver);
        styleOpt.state|=QStyle::State_Selected|QStyle::State_Enabled|QStyle::State_Active;
        styleOpt.viewItemPosition = QStyleOptionViewItemV4::OnlyOne;
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

// Copied from musique
QString GtkStyle::themeName()
{
    #ifdef Q_OS_WIN
    return QString();
    #else
    static QString name;

    if (name.isEmpty()) {
        QProcess process;
        process.start("dconf",  QStringList() << "read" << "/org/gnome/desktop/interface/gtk-theme");
        if (process.waitForFinished()) {
            name = process.readAllStandardOutput();
            name = name.trimmed();
            name.remove('\'');
            if (!name.isEmpty()) {
                return name;
            }
        }

        QString rcPaths = QString::fromLocal8Bit(qgetenv("GTK2_RC_FILES"));
        if (!rcPaths.isEmpty()) {
            QStringList paths = rcPaths.split(QLatin1String(":"));
            foreach (const QString &rcPath, paths) {
                if (!rcPath.isEmpty()) {
                    QFile rcFile(rcPath);
                    if (rcFile.exists() && rcFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                        QTextStream in(&rcFile);
                        while(!in.atEnd()) {
                            QString line = in.readLine();
                            if (line.contains(QLatin1String("gtk-theme-name"))) {
                                line = line.right(line.length() - line.indexOf(QLatin1Char('=')) - 1);
                                line.remove(QLatin1Char('\"'));
                                line = line.trimmed();
                                name = line;
                                break;
                            }
                        }
                    }
                }
                if (!name.isEmpty()) {
                    break;
                }
            }
        }

         #if QT_VERSION < 0x050000
        // Fall back to gconf
        if (name.isEmpty()) {
            name = QGtkStyle::getGConfString(QLatin1String("/desktop/gnome/interface/gtk_theme"));
        }
        #endif
    }
    return name;
    #endif
}

#ifndef Q_OS_WIN
static GtkProxyStyle *style=0;
#endif
static bool symbolicIcons=false;

void GtkStyle::applyTheme(QWidget *widget)
{
    #if defined Q_OS_WIN
    Q_UNUSED(widget)
    #else
    if (widget && isActive()) {
        QString theme=GtkStyle::themeName().toLower();
        GtkProxyStyle::ScrollbarType sbType=GtkProxyStyle::SB_Standard;
        if (!theme.isEmpty()) {
            QFile cssFile(QLatin1String(INSTALL_PREFIX"/share/")+QCoreApplication::applicationName()+"/"+theme+QLatin1String(".css"));
            if (cssFile.open(QFile::ReadOnly)) {
                QString css=QLatin1String(cssFile.readAll());
                QString header=css.left(100);
                qApp->setStyleSheet(css);
                if (header.contains("drag:toolbar")) {
                    WindowManager *wm=new WindowManager(widget);
                    wm->initialize(WindowManager::WM_DRAG_MENU_AND_TOOLBAR);
                    wm->registerWidgetAndChildren(widget);
                }
                #ifdef ENABLE_OVERLAYSCROLLBARS
                if (header.contains("scrollbar:overlay")) {
                    sbType=GtkProxyStyle::SB_Overlay;
                } else if (header.contains("scrollbar:thin")) {
                    sbType=GtkProxyStyle::SB_Thin;
                }
                #else
                if (header.contains("scrollbar:overlay") || header.contains("scrollbar:thin")) {
                    sbType=GtkProxyStyle::SB_Thin;
                }
                #endif
                symbolicIcons=header.contains("symbolic-icons:true");
            }
        }
        if (!style) {
            style=new GtkProxyStyle(sbType);
            qApp->setStyle(style);
        }
    }
    #endif
}

void GtkStyle::cleanup()
{
    #if !defined Q_OS_WIN && defined ENABLE_OVERLAYSCROLLBARS
    if (style) {
        style->destroySliderThumb();
    }
    #endif
}

bool GtkStyle::useSymbolicIcons()
{
    return symbolicIcons;
}
