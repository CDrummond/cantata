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

#include "shortcutssettingspage.h"
#include "mediakeys.h"
#include "toolbutton.h"
#include "actioncollection.h"
#include "shortcutssettingswidget.h"
#include "basicitemdelegate.h"
#include "localize.h"
#include "settings.h"
#include "icons.h"
#include <QBoxLayout>
#include <QComboBox>
#include <QGroupBox>
#include <QFormLayout>
#include <QProcess>

ShortcutsSettingsPage::ShortcutsSettingsPage(QWidget *p)
    : QWidget(p)
    , mediaKeysIfaceCombo(0)
    , settingsButton(0)
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
    QBoxLayout *boxLay=new QBoxLayout(QBoxLayout::LeftToRight, box);
    mediaKeysIfaceCombo=new QComboBox(box);
    boxLay->addWidget(mediaKeysIfaceCombo);
    mediaKeysIfaceCombo->addItem(i18n("Disabled"), (unsigned int)MediaKeys::NoInterface);
    #if QT_VERSION < 0x050000
    mediaKeysIfaceCombo->addItem(i18n("Enabled"), (unsigned int)MediaKeys::QxtInterface);
    #endif

    #if !defined Q_OS_WIN
    QByteArray desktop=qgetenv("XDG_CURRENT_DESKTOP");
    mediaKeysIfaceCombo->addItem(desktop=="Unity" || desktop=="GNOME"
                   ? i18n("Use desktop settings")
                   : i18n("Use GNOME/Unity settings"), (unsigned int)MediaKeys::GnomeInteface);

    settingsButton=new ToolButton(box);
    settingsButton->setToolTip(i18n("Configure..."));
    settingsButton->setIcon(Icons::self()->configureIcon);
    boxLay->addWidget(settingsButton);
    connect(settingsButton, SIGNAL(clicked(bool)), SLOT(showGnomeSettings()));
    connect(mediaKeysIfaceCombo, SIGNAL(currentIndexChanged(int)), SLOT(mediaKeysIfaceChanged()));
    #endif

    lay->addWidget(box);
    #endif // QT_VERSION < 0x050000 || !defined Q_OS_WIN

    #endif // !defined Q_OS_MAC
}

void ShortcutsSettingsPage::load()
{
    if (!mediaKeysIfaceCombo) {
        return;
    }
    unsigned int iface=(unsigned int)MediaKeys::toIface(Settings::self()->mediaKeysIface());
    for (int i=0; i<mediaKeysIfaceCombo->count(); ++i) {
        if (mediaKeysIfaceCombo->itemData(i).toUInt()==iface) {
            mediaKeysIfaceCombo->setCurrentIndex(i);
            break;
        }
    }
}

void ShortcutsSettingsPage::save()
{
    shortcuts->save();
    if (mediaKeysIfaceCombo) {
        Settings::self()->saveMediaKeysIface(MediaKeys::toString((MediaKeys::InterfaceType)mediaKeysIfaceCombo->itemData(mediaKeysIfaceCombo->currentIndex()).toUInt()));
    }
}

void ShortcutsSettingsPage::mediaKeysIfaceChanged()
{
    if (!settingsButton) {
        return;
    }
    settingsButton->setEnabled(MediaKeys::GnomeInteface==mediaKeysIfaceCombo->itemData(mediaKeysIfaceCombo->currentIndex()).toUInt());
}

void ShortcutsSettingsPage::showGnomeSettings()
{
    QProcess::startDetached("gnome-control-center", QStringList() << "keyboard");
}
