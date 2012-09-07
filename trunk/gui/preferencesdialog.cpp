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

#include "preferencesdialog.h"
#include "mainwindow.h"
#include "settings.h"
#include "icon.h"
#include "interfacesettings.h"
#include "externalsettings.h"
#include "serversettings.h"
#include "serverplaybacksettings.h"
#include "playbacksettings.h"
#ifdef TAGLIB_FOUND
#include "httpserversettings.h"
#endif
#include "lyricsettings.h"
#include "lyricspage.h"
#include "localize.h"
#include "mpdconnection.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KPageWidget>
#include <KDE/KIcon>
#else
#include "fancytabwidget.h"
#include "proxysettings.h"
#endif

#ifndef ENABLE_KDE_SUPPORT
class ConfigPage : public QWidget
{
public:
    ConfigPage(QWidget *p, const QString &title, const QIcon &icon, QWidget *cfg) : QWidget(p)
    {
        static int size=-1;

        if (-1==size) {
            size=QApplication::fontMetrics().height();
            if (size>20) {
                size=Icon::stdSize(size*1.25);
            } else {
                size=22;
            }
        }

        QBoxLayout *layout=new QBoxLayout(QBoxLayout::TopToBottom, this);
        QBoxLayout *titleLayout=new QBoxLayout(QBoxLayout::LeftToRight, 0);
        titleLayout->addWidget(new QLabel("<b>"+title+"</b>", this));
        titleLayout->addItem(new QSpacerItem(16, 16, QSizePolicy::Expanding, QSizePolicy::Minimum));

        QLabel *icn=new QLabel(this);
        icn->setPixmap(icon.pixmap(size, size));
        titleLayout->addWidget(icn);
        layout->addLayout(titleLayout);
        layout->addItem(new QSpacerItem(8, 8, QSizePolicy::Fixed, QSizePolicy::Fixed));
        layout->addWidget(cfg);
        QSizePolicy sizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(cfg->sizePolicy().hasHeightForWidth());
        cfg->setSizePolicy(sizePolicy);
        cfg->setParent(this);
    }
};
#endif

PreferencesDialog::PreferencesDialog(QWidget *parent, LyricsPage *lp)
    : Dialog(parent)
{
    setButtons(Ok|Apply|Cancel);

    #ifdef ENABLE_KDE_SUPPORT
    KPageWidget *widget = new KPageWidget(this);
    #else
    FancyTabWidget *widget = new FancyTabWidget(this, false, true);
    #endif

    server = new ServerSettings(widget);
    serverplayback = new ServerPlaybackSettings(widget);
    playback = new PlaybackSettings(widget);
    interface = new InterfaceSettings(widget);
    ext = new ExternalSettings(widget);
    #ifdef TAGLIB_FOUND
    http = new HttpServerSettings(widget);
    #endif
    lyrics = new LyricSettings(widget);
    server->load();
    serverplayback->load();
    playback->load();
    interface->load();
    ext->load();
    #ifdef TAGLIB_FOUND
    http->load();
    #endif
    const QList<UltimateLyricsProvider *> &lprov=lp->getProviders();
    lyrics->Load(lprov);
    #ifdef ENABLE_KDE_SUPPORT
    KPageWidgetItem *page=widget->addPage(server, i18n("Connection"));
    page->setHeader(i18n("Connection Settings"));
    page->setIcon(KIcon("server-database"));
    page=widget->addPage(serverplayback, i18n("Output"));
    page->setHeader(i18n("Output Settings"));
    page->setIcon(KIcon("speaker"));
    page=widget->addPage(playback, i18n("Playback"));
    page->setHeader(i18n("Playback Settings"));
    page->setIcon(KIcon("media-playback-start"));
    page=widget->addPage(interface, i18n("Interface"));
    page->setHeader(i18n("Interface Settings"));
    page->setIcon(KIcon("view-choose"));
    page=widget->addPage(ext, i18n("External"));
    page->setHeader(i18n("External Settings"));
    page->setIcon(KIcon("video-display"));
    #ifdef TAGLIB_FOUND
    page=widget->addPage(http, i18n("HTTP Server"));
    page->setHeader(i18n("HTTP Server Settings"));
    #endif
    page->setIcon(KIcon("network-server"));
    page=widget->addPage(lyrics, i18n("Lyrics"));
    page->setHeader(i18n("Lyrics Settings"));
    page->setIcon(KIcon("view-media-lyrics"));
    #else
    widget->AddTab(new ConfigPage(this, i18n("Connection Settings"), Icon("server-database"), server),
                   Icon("server-database"), i18n("Connection"));
    widget->AddTab(new ConfigPage(this, i18n("Output Settings"), Icon("speaker"), serverplayback),
                   Icon("speaker"), i18n("Output"));
    widget->AddTab(new ConfigPage(this, i18n("Playback Settings"), Icon("media-playback-start"), playback),
                   Icon("media-playback-start"), i18n("Playback"));
    widget->AddTab(new ConfigPage(this, i18n("Interface Settings"), Icon("view-choose"), interface),
                   Icon("view-choose"), i18n("Interface"));
    widget->AddTab(new ConfigPage(this, i18n("External Settings"), Icon("video-display"), ext),
                   Icon("video-display"), i18n("External"));
    #ifdef TAGLIB_FOUND
    widget->AddTab(new ConfigPage(this, i18n("HTTP Server Settings"), Icon("network-server"), http),
                   Icon("network-server"), i18n("HTTP Server"));
    #endif
    widget->AddTab(new ConfigPage(this, i18n("Lyrics Settings"), Icon("view-media-lyrics"), lyrics),
                   Icon("view-media-lyrics"), i18n("Lyrics"));
    proxy = new ProxySettings(this);
    proxy->load();
    widget->AddTab(new ConfigPage(this, i18nc("Qt-only", "Proxy Settings"), Icon("preferences-system-network"), proxy),
                   Icon("preferences-system-network"), i18nc("Qt-only", "Proxy"));
    widget->SetMode(FancyTabWidget::Mode_LargeSidebar);
    #endif
    setCaption(i18n("Configure"));
    setMainWidget(widget);
    connect(server, SIGNAL(connectTo(const MPDConnectionDetails &)), SIGNAL(connectTo(const MPDConnectionDetails &)));
    connect(server, SIGNAL(disconnectFromMpd()), MPDConnection::self(), SLOT(disconnectMpd()));
}

void PreferencesDialog::writeSettings()
{
    // *Must* save server settings first, so that MPD settings go to the correct instance!
    server->save();
    serverplayback->save();
    playback->save();
    interface->save();
    ext->save();
    #ifdef TAGLIB_FOUND
    http->save();
    #endif
    #ifndef ENABLE_KDE_SUPPORT
    proxy->save();
    #endif
    Settings::self()->saveLyricProviders(lyrics->EnabledProviders());
    Settings::self()->save();
    emit settingsSaved();
}

void PreferencesDialog::slotButtonClicked(int button)
{
    switch (button) {
    case Ok:
    case Apply:
        writeSettings();
        break;
    case Cancel:
        reject();
        break;
    default:
        break;
    }

    if (Ok==button) {
        accept();
    }

    Dialog::slotButtonClicked(button);
}

