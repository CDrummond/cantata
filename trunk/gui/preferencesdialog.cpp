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

#include "preferencesdialog.h"
#include "mainwindow.h"
#include "settings.h"
#include "icons.h"
#include "interfacesettings.h"
#include "streamssettings.h"
#include "onlinesettings.h"
#include "serversettings.h"
#include "playbacksettings.h"
#include "filesettings.h"
#ifdef TAGLIB_FOUND
#include "httpserversettings.h"
#endif
#include "contextsettings.h"
#include "cachesettings.h"
#include "localize.h"
#include "mpdconnection.h"
#include "pagewidget.h"
#ifdef ENABLE_PROXY_CONFIG
#include "proxysettings.h"
#endif
#ifndef ENABLE_KDE_SUPPORT
#include "shortcutssettingspage.h"
#include "actioncollection.h"
#include "basicitemdelegate.h"
#endif
#if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
#include "audiocdsettings.h"
#endif
#include "mediakeys.h"
#include <QBoxLayout>
#include <QComboBox>
#include <QGroupBox>
#include <QFormLayout>

static int iCount=0;

int PreferencesDialog::instanceCount()
{
    return iCount;
}

#ifndef ENABLE_KDE_SUPPORT
class ShortcutsSettingsPage : public QWidget
{
public:
    ShortcutsSettingsPage(QWidget *p)
        : QWidget(p)
        , combo(0)
    {
        QBoxLayout *lay=new QBoxLayout(QBoxLayout::TopToBottom, this);
        lay->setMargin(0);

        QHash<QString, ActionCollection *> map;
        map.insert("Cantata", ActionCollection::get());
        shortcuts = new ShortcutsSettingsWidget(map, this);
        shortcuts->view()->setAlternatingRowColors(false);
        shortcuts->view()->setItemDelegate(new BasicItemDelegate(shortcuts->view()));
        lay->addWidget(shortcuts);

        #if !defined Q_OS_MAC
        #if QT_VERSION < 0x050000 || !defined Q_OS_WIN
        QGroupBox *box=new QGroupBox(i18n("Multi-Media Keys"));
        QBoxLayout *boxLay=new QBoxLayout(QBoxLayout::TopToBottom, box);
        combo=new QComboBox(box);
        boxLay->addWidget(combo);
        combo->addItem(i18n("Disabled"), (unsigned int)MediaKeys::NoInterface);
        #if QT_VERSION < 0x050000
        combo->addItem(i18n("Enabled"), (unsigned int)MediaKeys::QxtInterface);
        #endif
        #if !defined Q_OS_WIN
        QByteArray desktop=qgetenv("XDG_CURRENT_DESKTOP");
        combo->addItem(desktop=="Unity" || desktop=="GNOME"
                            ? i18n("Use desktop settings")
                            : i18n("Use GNOME/Unity settings"), (unsigned int)MediaKeys::GnomeInteface);
        #endif
        lay->addWidget(box);
        #endif // QT_VERSION < 0x050000 || !defined Q_OS_WIN
        #endif // !defined Q_OS_MAC
    }

    void load()
    {
        if (!combo) {
            return;
        }
        unsigned int iface=(unsigned int)MediaKeys::toIface(Settings::self()->mediaKeysIface());
        for (int i=0; i<combo->count(); ++i) {
            if (combo->itemData(i).toUInt()==iface) {
                combo->setCurrentIndex(i);
                break;
            }
        }
    }

    void save()
    {
        shortcuts->save();
        if (combo) {
            Settings::self()->saveMediaKeysIface(MediaKeys::toString((MediaKeys::InterfaceType)combo->itemData(combo->currentIndex()).toUInt()));
        }
    }

private:
    ShortcutsSettingsWidget *shortcuts;
    QComboBox *combo;
};
#endif

PreferencesDialog::PreferencesDialog(QWidget *parent)
    : Dialog(parent, "PreferencesDialog")
{
    iCount++;
    setButtons(Ok|Apply|Cancel);

    PageWidget *widget = new PageWidget(this);
    server = new ServerSettings(0);
    playback = new PlaybackSettings(0);
    files = new FileSettings(0);
    interface = new InterfaceSettings(0);
    streams = new StreamsSettings(0);
    online = new OnlineSettings(0);
    context = new ContextSettings(0);
    cache = new CacheSettings(0);
    server->load();
    playback->load();
    files->load();
    interface->load();
    streams->load();
    online->load();
    context->load();
    widget->addPage(server, i18n("Collection"), Icons::self()->libraryIcon, i18n("Collection Settings"));
    widget->addPage(playback, i18n("Playback"), Icon("media-playback-start"), i18n("Playback Settings"));
    widget->addPage(files, i18n("Files"), Icons::self()->filesIcon, i18n("File Settings"));
    widget->addPage(interface, i18n("Interface"), Icon("preferences-other"), i18n("Interface Settings"));
    widget->addPage(streams, i18n("Streams"), Icons::self()->radioStreamIcon, i18n("Streams Settings"));
    widget->addPage(online, i18n("Online"), Icon("applications-internet"), i18n("Online Providers"));
    widget->addPage(context, i18n("Context"), Icons::self()->contextIcon, i18n("Context View Settings"));
    #ifdef TAGLIB_FOUND
    http = new HttpServerSettings(0);
    if (http->haveMultipleInterfaces()) {
        http->load();
        widget->addPage(http, i18n("HTTP Server"), Icon("network-server"), i18n("HTTP Server Settings"));
    } else {
        http->deleteLater();
        http=0;
    }
    #endif
    #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
    audiocd = new AudioCdSettings(0);
    audiocd->load();
    widget->addPage(audiocd, i18n("Audio CD"), Icon("media-optical"), i18n("Audio CD Settings"));
    #endif
    #ifdef ENABLE_PROXY_CONFIG
    proxy = new ProxySettings(0);
    proxy->load();
    widget->addPage(proxy, i18n("Proxy"), Icon("preferences-system-network"), i18nc("Qt-only", "Proxy Settings"));
    #endif
    #ifndef ENABLE_KDE_SUPPORT
    shortcuts = new ShortcutsSettingsPage(widget);
    widget->addPage(shortcuts, i18nc("Qt-only", "Shortcuts"), Icons::self()->shortcutsIcon, i18nc("Qt-only", "Keyboard Shortcut Settings"));
    shortcuts->load();
    #endif
    widget->addPage(cache, i18n("Cache"), Icon("folder"), i18n("Cached Items"));
    setCaption(i18n("Configure"));
    setMainWidget(widget);
    setAttribute(Qt::WA_DeleteOnClose);
    connect(files, SIGNAL(reloadStreams()), SIGNAL(reloadStreams()));
    #ifndef ENABLE_KDE_SUPPORT
    int h=sizeHint().height();
    setMinimumHeight(h);
    setMinimumWidth(h);
    #endif
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
}

PreferencesDialog::~PreferencesDialog()
{
    iCount--;
}

void PreferencesDialog::writeSettings()
{
    // *Must* save server settings first, so that MPD settings go to the correct instance!
    server->save();
    playback->save();
    files->save();
    interface->save();
    streams->save();
    online->save();
    #ifdef TAGLIB_FOUND
    if (http) {
        http->save();
    }
    #endif
    #ifndef ENABLE_KDE_SUPPORT
    #ifdef ENABLE_PROXY_CONFIG
    proxy->save();
    #endif
    shortcuts->save();
    #endif
    #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
    audiocd->save();
    #endif
    context->save();
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
        server->cancel();
        reject();
        // Need to call this - if not, when dialog is closed by window X control, it is not deleted!!!!
        Dialog::slotButtonClicked(button);
        break;
    default:
        break;
    }

    if (Ok==button) {
        accept();
    }

    Dialog::slotButtonClicked(button);
}
