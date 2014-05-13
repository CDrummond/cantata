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

#include "shortcutssettingspage.h"
#include "mediakeys.h"
#include "widgets/toolbutton.h"
#include "support/actioncollection.h"
#include "support/shortcutssettingswidget.h"
#include "widgets/basicitemdelegate.h"
#include "support/localize.h"
#include "settings.h"
#include "widgets/icons.h"
#include "support/buddylabel.h"
#include "support/utils.h"
#include "support/flickcharm.h"
#include <QBoxLayout>
#include <QComboBox>
#include <QGroupBox>
#include <QFormLayout>
#include <QCheckBox>
#include <QProcess>

static const char * constMkEnabledVal="mk-enabled-val";

ShortcutsSettingsPage::ShortcutsSettingsPage(QWidget *p)
    : QWidget(p)
    , mediaKeysIfaceCombo(0)
    , settingsButton(0)
    , mediaKeysEnabled(0)
{
    QBoxLayout *lay=new QBoxLayout(QBoxLayout::TopToBottom, this);
    lay->setMargin(0);

    QHash<QString, ActionCollection *> map;
    map.insert("Cantata", ActionCollection::get());
    shortcuts = new ShortcutsSettingsWidget(map, this);
    shortcuts->view()->setAlternatingRowColors(false);
    shortcuts->view()->setItemDelegate(new BasicItemDelegate(shortcuts->view()));
    FlickCharm::self()->activateOn(shortcuts->view());
    lay->addWidget(shortcuts);

    #if !defined Q_OS_WIN && !defined Q_OS_MAC
    Utils::Desktop de=Utils::currentDe();
    bool useDesktop=true;
    bool isGnome=Utils::Unity==de || Utils::Gnome==de;
    #else
    bool useDesktop=false;
    bool isGnome=false;
    #endif

    #ifdef CANTATA_USE_QXT_MEDIAKEYS
    bool useQxt=true;
    #else
    bool useQxt=false;
    #endif

    if (useDesktop || useQxt) {
        QGroupBox *box=new QGroupBox(i18n("Multi-Media Keys"));

        if (useDesktop && useQxt) {
            QBoxLayout *boxLay=new QBoxLayout(QBoxLayout::LeftToRight, box);
            mediaKeysIfaceCombo=new QComboBox(box);
            boxLay->addWidget(mediaKeysIfaceCombo);
            mediaKeysIfaceCombo->addItem(i18n("Do not use media keys to control Cantata"), (unsigned int)MediaKeys::NoInterface);
            mediaKeysIfaceCombo->addItem(i18n("Use media keys to control Cantata"), (unsigned int)MediaKeys::QxtInterface);

            mediaKeysIfaceCombo->addItem(isGnome
                                         ? i18n("Use media keys, as configured in desktop settings, to control Cantata")
                                         : i18n("Use media keys, as configured in GNOME/Unity settings, to control Cantata"), (unsigned int)MediaKeys::GnomeInteface);

            settingsButton=new ToolButton(box);
            settingsButton->setToolTip(i18n("Configure..."));
            settingsButton->setIcon(Icons::self()->configureIcon);
            boxLay->addWidget(settingsButton);
            boxLay->addItem(new QSpacerItem(0, 1, QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
            connect(settingsButton, SIGNAL(clicked(bool)), SLOT(showGnomeSettings()));
            connect(mediaKeysIfaceCombo, SIGNAL(currentIndexChanged(int)), SLOT(mediaKeysIfaceChanged()));
        } else if (useQxt) {
            QBoxLayout *boxLay=new QBoxLayout(QBoxLayout::LeftToRight, box);
            mediaKeysEnabled = new QCheckBox(i18n("Use media keys to control Cantata"), box);
            boxLay->addWidget(mediaKeysEnabled);
            boxLay->addItem(new QSpacerItem(0, 1, QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
            mediaKeysEnabled->setProperty(constMkEnabledVal, (unsigned int)MediaKeys::QxtInterface);
        } else if (useDesktop) {
            QBoxLayout *boxLay=new QBoxLayout(QBoxLayout::LeftToRight, box);
            QWidget *controlWidget = new QWidget(box);
            mediaKeysEnabled = new QCheckBox(isGnome
                                             ? i18n("Use media keys, as configured in desktop settings, to control Cantata")
                                             : i18n("Use media keys, as configured in GNOME/Unity settings, to control Cantata"), controlWidget);
            settingsButton=new ToolButton(controlWidget);
            settingsButton->setToolTip(i18n("Configure..."));
            settingsButton->setIcon(Icons::self()->configureIcon);
            boxLay->addWidget(mediaKeysEnabled);
            boxLay->addWidget(settingsButton);
            boxLay->addItem(new QSpacerItem(0, 1, QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
            mediaKeysEnabled->setProperty(constMkEnabledVal, (unsigned int)MediaKeys::GnomeInteface);
            connect(mediaKeysEnabled, SIGNAL(toggled(bool)), settingsButton, SLOT(setEnabled(bool)));
            connect(settingsButton, SIGNAL(clicked(bool)), SLOT(showGnomeSettings()));
        }
        lay->addWidget(box);
    }
}

void ShortcutsSettingsPage::load()
{
    if (settingsButton) {
        settingsButton->setEnabled(false);
    }
    if (mediaKeysIfaceCombo) {
        unsigned int iface=(unsigned int)MediaKeys::toIface(Settings::self()->mediaKeysIface());
        for (int i=0; i<mediaKeysIfaceCombo->count(); ++i) {
            if (mediaKeysIfaceCombo->itemData(i).toUInt()==iface) {
                mediaKeysIfaceCombo->setCurrentIndex(i);
                if (settingsButton) {
                    settingsButton->setEnabled(iface==MediaKeys::GnomeInteface);
                }
                break;
            }
        }
    } else if (mediaKeysEnabled) {
        mediaKeysEnabled->setChecked(MediaKeys::NoInterface!=MediaKeys::toIface(Settings::self()->mediaKeysIface()));
        if (settingsButton) {
            settingsButton->setEnabled(mediaKeysEnabled->isChecked());
        }
    }
}

void ShortcutsSettingsPage::save()
{
    shortcuts->save();
    if (mediaKeysIfaceCombo) {
        Settings::self()->saveMediaKeysIface(MediaKeys::toString((MediaKeys::InterfaceType)mediaKeysIfaceCombo->itemData(mediaKeysIfaceCombo->currentIndex()).toUInt()));
    } else if (mediaKeysEnabled) {
        Settings::self()->saveMediaKeysIface(MediaKeys::toString(
                                                mediaKeysEnabled->isChecked()
                                                    ? (MediaKeys::InterfaceType)mediaKeysEnabled->property(constMkEnabledVal).toUInt()
                                                    : MediaKeys::NoInterface));
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
    #if !defined Q_OS_WIN && !defined Q_OS_MAC
    if (Utils::Unity==Utils::currentDe() && !Utils::findExe("unity-control-center").isEmpty()) {
        QProcess::startDetached("unity-control-center", QStringList() << "keyboard");
        return;
    }
    #endif
    QProcess::startDetached("gnome-control-center", QStringList() << "keyboard");
}
