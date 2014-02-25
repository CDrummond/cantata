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
#ifndef ENABLE_KDE_SUPPORT
#include "proxystyle.h"
#endif
#if !defined Q_OS_WIN && !defined QT_NO_STYLE_GTK
#include "gtkproxystyle.h"
#include "windowmanager.h"
#if QT_VERSION < 0x050000
#include <QGtkStyle>
#endif
#endif

static bool usingGtkStyle=false;

static inline void setup()
{
    #if !defined Q_OS_WIN && !defined QT_NO_STYLE_GTK
    static bool init=false;
    if (!init) {
        init=true;
        usingGtkStyle=QApplication::style()->inherits("QGtkStyle");
    }
    #endif
}

bool GtkStyle::isActive()
{
    setup();
    return usingGtkStyle;
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

#if !defined Q_OS_WIN && !defined QT_NO_STYLE_GTK
// For some reason, dconf does not seem to terminate correctly when run under some desktops (e.g. KDE)
// Destroying the QProcess seems to block, causing the app to appear to hang before starting.
// So, create QProcess on the heap - and only wait 1.5s for response. Connect finished to deleteLater
// so that the object is destroyed.
static QString runProc(const QString &cmd, const QStringList &args)
{
    QProcess *process=new QProcess();
    process->start(cmd, args);
    QObject::connect(process, SIGNAL(finished(int)), process, SLOT(deleteLater()));

    if (process->waitForFinished(1500)) {
        QString resp = process->readAllStandardOutput();
        resp = resp.trimmed();
        resp.remove('\'');
        return resp;
    }
    process->kill();
    return QString();
}
#endif

#if !defined Q_OS_WIN && !defined QT_NO_STYLE_GTK
static QString iconThemeSetting;
static QString themeNameSetting;
#endif

// Copied from musique
QString GtkStyle::themeName()
{
    #if defined Q_OS_WIN || defined QT_NO_STYLE_GTK
    return QString();
    #else

    if (themeNameSetting.isEmpty()) {
        static bool read=false;

        if (read) {
            return themeNameSetting;
        }
        read=true;

        if (themeNameSetting.isEmpty()) {
            themeNameSetting=runProc("dconf",  QStringList() << "read" << "/org/gnome/desktop/interface/gtk-theme");
            if (!themeNameSetting.isEmpty()) {
                return themeNameSetting;
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
                                    themeNameSetting = line;
                                    break;
                                }
                            }
                        }
                    }
                    if (!themeNameSetting.isEmpty()) {
                        break;
                    }
                }
            }

            #if QT_VERSION < 0x050000
            // Fall back to gconf
            if (themeNameSetting.isEmpty()) {
                themeNameSetting = QGtkStyle::getGConfString(QLatin1String("/desktop/gnome/interface/gtk_theme"));
            }
            #endif
        }
    }
    return themeNameSetting;
    #endif
}

QString GtkStyle::iconTheme()
{
    #if defined Q_OS_WIN || defined QT_NO_STYLE_GTK
    return QString();
    #else
    if (iconThemeSetting.isEmpty()) {
        static bool read=false;

        if (!read) {
            read=true;
            iconThemeSetting=runProc("dconf",  QStringList() << "read" << "/org/gnome/desktop/interface/icon-theme");
        }
    }
    return iconThemeSetting;
    #endif
}

extern void GtkStyle::setThemeName(const QString &n)
{
    #if defined Q_OS_WIN || defined QT_NO_STYLE_GTK
    Q_UNUSED(n)
    #else
    themeNameSetting=n;
    #endif
}

extern void GtkStyle::setIconTheme(const QString &n)
{
    #if defined Q_OS_WIN || defined QT_NO_STYLE_GTK
    Q_UNUSED(n)
    #else
    iconThemeSetting=n;
    #endif
}

#if !defined Q_OS_WIN && !defined QT_NO_STYLE_GTK
static GtkProxyStyle *gtkProxyStyle=0;
#endif
#ifndef ENABLE_KDE_SUPPORT
static ProxyStyle *plainProxyStyle=0;
#endif
static bool symbolicIcons=false;
static QColor symbolicIconColor(0, 0, 0);

void GtkStyle::applyTheme(QWidget *widget)
{
    #if defined Q_OS_WIN || defined QT_NO_STYLE_GTK
    Q_UNUSED(widget)
    #ifndef ENABLE_KDE_SUPPORT
    if (!plainProxyStyle) {
        plainProxyStyle=new ProxyStyle();
        qApp->setStyle(plainProxyStyle);
    }
    #endif
    #else
    if (widget && isActive()) {
        QString theme=GtkStyle::themeName().toLower();
        GtkProxyStyle::ScrollbarType sbType=GtkProxyStyle::SB_Standard;
        bool touchStyleSpin=false;
        QMap<QString, QString> css;
        WindowManager *wm=0;
        if (!theme.isEmpty()) {
            QFile cssFile(QLatin1String(INSTALL_PREFIX"/share/")+QCoreApplication::applicationName()+QLatin1String("/themes/")+theme+QLatin1String(".css"));
            if (cssFile.open(QFile::ReadOnly|QFile::Text)) {
                const QString symKey=QLatin1String("symbolic-icons:#");

                while (!cssFile.atEnd()) {
                    QString line = cssFile.readLine().trimmed();
                    if (line.isEmpty()) {
                        continue;
                    }
                    if (line.startsWith(QLatin1String("/*"))) {
                        if (!wm && line.contains("drag:toolbar")) {
                            wm=new WindowManager(widget);
                            wm->initialize(WindowManager::WM_DRAG_MENU_AND_TOOLBAR);
                            wm->registerWidgetAndChildren(widget);
                        }
                        if (line.contains("scrollbar:overlay") || line.contains("scrollbar:thin")) {
                            sbType=GtkProxyStyle::SB_Thin;
                        }
                        touchStyleSpin=line.contains("spinbox:touch");
                        int pos=line.indexOf(symKey);
                        if (pos>0 && pos+6<line.length()) {
                            symbolicIcons=true;
                            symbolicIconColor=QColor(line.mid(pos+symKey.length()-1, 7));
                        }
                    } else {
                        int space=line.indexOf(' ');
                        if (space>2) {
                            QString key=line.left(space);
                            css.insert(line.left(space), line);
                        }
                    }
                }
            }
        }
        if (!gtkProxyStyle) {
            gtkProxyStyle=new GtkProxyStyle(sbType, touchStyleSpin, css);
            qApp->setStyle(gtkProxyStyle);
            QCoreApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
        }
    }

    #ifndef ENABLE_KDE_SUPPORT
    if (!gtkProxyStyle && !plainProxyStyle) {
        plainProxyStyle=new ProxyStyle();
        qApp->setStyle(plainProxyStyle);
    }
    #endif
    #endif
}

bool GtkStyle::useSymbolicIcons()
{
    return symbolicIcons;
}

QColor GtkStyle::symbolicColor()
{
    return symbolicIconColor;
}
