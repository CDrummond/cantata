/*
 * Cantata
 *
 * Copyright (c) 2011-2015 Craig Drummond <craig.p.drummond@gmail.com>
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
#include <QStyleOptionViewItemV4>
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
#include "windowmanager.h"
#if QT_VERSION < 0x050000
#include <QGtkStyle>
#endif
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

QString GtkStyle::readDconfSetting(const QString &setting, const QString &scheme)
{
    #ifdef NO_GTK_SUPPORT
    Q_UNUSED(setting)
    Q_UNUSED(scheme)
    #else
    // For some reason, dconf does not seem to terminate correctly when run under some desktops (e.g. KDE)
    // Destroying the QProcess seems to block, causing the app to appear to hang before starting.
    // So, create QProcess on the heap - and only wait 1.5s for response. Connect finished to deleteLater
    // so that the object is destroyed.
    static bool first=true;
    static bool ok=true;

    if (first) {
        first=false;
    } else if (!ok) { // Failed before, so dont bothe rcalling dconf again!
        return QString();
    }

    QString schemeToUse=scheme.isEmpty() ? QLatin1String("/org/gnome/desktop/interface/") : scheme;
    QProcess *process=new QProcess();
    process->start(QLatin1String("dconf"), QStringList() << QLatin1String("read") << schemeToUse+setting);
    QObject::connect(process, SIGNAL(finished(int)), process, SLOT(deleteLater()));

    if (process->waitForFinished(1500)) {
        QString resp = process->readAllStandardOutput();
        resp = resp.trimmed();
        resp.remove('\'');

        if (resp.isEmpty()) {
            // Probably set to the default, and dconf does not store defaults! Therefore, need to read via gsettings...
            schemeToUse=schemeToUse.mid(1, schemeToUse.length()-2).replace("/", ".");
            QProcess *gsettingsProc=new QProcess();
            gsettingsProc->start(QLatin1String("gsettings"), QStringList() << QLatin1String("get") << schemeToUse << setting);
            QObject::connect(gsettingsProc, SIGNAL(finished(int)), process, SLOT(deleteLater()));
            if (gsettingsProc->waitForFinished(1500)) {
                resp = gsettingsProc->readAllStandardOutput();
                resp = resp.trimmed();
                resp.remove('\'');
            } else {
                gsettingsProc->kill();
            }
        }
        return resp;
    } else { // If we failed 1 time, dont bother next time!
        ok=false;
    }
    process->kill();
    #endif
    return QString();
}

#ifndef NO_GTK_SUPPORT
static QString iconThemeSetting;
static QString themeNameSetting;
#endif

// Copied from musique
QString GtkStyle::themeName()
{
    #ifdef NO_GTK_SUPPORT
    return QString();
    #else

    if (themeNameSetting.isEmpty()) {
        static bool read=false;

        if (read) {
            return themeNameSetting;
        }
        read=true;

        if (themeNameSetting.isEmpty()) {
            themeNameSetting=readDconfSetting(QLatin1String("gtk-theme"));
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
            if (themeNameSetting.isEmpty() && Utils::Unity==Utils::currentDe()) {
                themeNameSetting=QLatin1String("Ambiance");
            }
        }
    }
    return themeNameSetting;
    #endif
}

QString GtkStyle::iconTheme()
{
    #ifdef NO_GTK_SUPPORT
    return QString();
    #else
    if (iconThemeSetting.isEmpty()) {
        static bool read=false;

        if (!read) {
            read=true;
            iconThemeSetting=readDconfSetting(QLatin1String("icon-theme"));
            if (iconThemeSetting.isEmpty() && Utils::Unity==Utils::currentDe()) {
                iconThemeSetting=QLatin1String("ubuntu-mono-dark");
            }
        }
    }
    return iconThemeSetting;
    #endif
}

extern void GtkStyle::setThemeName(const QString &n)
{
    #ifdef NO_GTK_SUPPORT
    Q_UNUSED(n)
    #else
    themeNameSetting=n;
    #endif
}

extern void GtkStyle::setIconTheme(const QString &n)
{
    #ifdef NO_GTK_SUPPORT
    Q_UNUSED(n)
    #else
    iconThemeSetting=n;
    #endif
}

#ifndef NO_GTK_SUPPORT
static WindowManager *wm=0;
#endif
static QProxyStyle *proxyStyle=0;
static bool symbolicIcons=false;
static QColor symbolicIconColor(0, 0, 0);
static bool thinSbar=false;

bool GtkStyle::thinScrollbars()
{
    return thinSbar;
}

void GtkStyle::applyTheme(QWidget *widget)
{
    #ifdef NO_GTK_SUPPORT
    Q_UNUSED(widget)
    #else
    if (widget && isActive()) {
        QString theme=GtkStyle::themeName().toLower();
        bool touchStyleSpin=false;
        int modViewFrame=0;
        QMap<QString, QString> css;
        if (!theme.isEmpty()) {
            QFile cssFile(Utils::systemDir(QLatin1String("themes"))+theme+QLatin1String(".css"));
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
                            thinSbar=true;
                        }
                        touchStyleSpin=line.contains("spinbox:touch");
                        modViewFrame=line.contains("modview:ts")
                                        ? ProxyStyle::VF_Side|ProxyStyle::VF_Top
                                        : line.contains("modview:true")
                                            ? ProxyStyle::VF_Side
                                            : 0;
                        int pos=line.indexOf(symKey);
                        if (pos>0 && pos+6<line.length()) {
                            symbolicIcons=true;
                            symbolicIconColor=QColor(line.mid(pos+symKey.length()-1, 7));
                        }
                    } else {
                        int space=line.indexOf(' ');
                        if (space>2) {
                            css.insert(line.left(space), line);
                        }
                    }
                }
            }
        }
        if (!proxyStyle) {
            proxyStyle=new GtkProxyStyle(modViewFrame, thinSbar, touchStyleSpin || Utils::touchFriendly(), css);
        }
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
    #ifndef ENABLE_KDE_SUPPORT
    if (!proxyStyle) {
        proxyStyle=new ProxyStyle(modViewFrame);
    }
    #endif
    if (proxyStyle) {
        qApp->setStyle(proxyStyle);
    }
}

void GtkStyle::registerWidget(QWidget *widget)
{
    #ifdef NO_GTK_SUPPORT
    Q_UNUSED(widget)
    #else
    if (widget && wm) {
        wm->registerWidgetAndChildren(widget);
    }
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
