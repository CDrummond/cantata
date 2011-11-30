/*
 * Copyright (c) 2008 Sander Knopper (sander AT knopper DOT tk) and
 *                    Roeland Douma (roeland AT rullzer DOT com)
 *
 * This file is part of QtMPC.
 *
 * QtMPC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * QtMPC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QtMPC.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "preferencesdialog.h"
#include "mainwindow.h"
#include "settings.h"
#include "interfacesettings.h"
#include "playbacksettings.h"
#include "outputsettings.h"
#include "serversettings.h"
#include "lyricsettings.h"
#include "lyricspage.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KPageWidget>
#include <KDE/KIcon>
#else
#include "fancytabwidget.h"
#include <QtGui/QDialogButtonBox>
#include "proxysettings.h"
#endif

#ifndef ENABLE_KDE_SUPPORT
class ConfigPage : public QWidget
{
public:
    ConfigPage(QWidget *p, const QString &title, const QIcon &icon, QWidget *cfg) : QWidget(p)
    {
        QBoxLayout *layout=new QBoxLayout(QBoxLayout::TopToBottom, this);
        QBoxLayout *titleLayout=new QBoxLayout(QBoxLayout::LeftToRight, this);
        titleLayout->addWidget(new QLabel("<b>"+title+"</b>", this));
        QLabel *icn=new QLabel(this);
        icn->setPixmap(icon.pixmap(22, 22));
        titleLayout->addItem(new QSpacerItem(16, 16, QSizePolicy::Expanding, QSizePolicy::Minimum));
        titleLayout->addWidget(icn);
        layout->addItem(titleLayout);
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

#ifdef ENABLE_KDE_SUPPORT
PreferencesDialog::PreferencesDialog(QWidget *parent, LyricsPage *lp)
    : KDialog(parent)
#else
PreferencesDialog::PreferencesDialog(QWidget *parent, LyricsPage *lp)
    : QDialog(parent)
#endif
{
#ifdef ENABLE_KDE_SUPPORT
    KPageWidget *widget = new KPageWidget(this);
#else
    FancyTabWidget *widget = new FancyTabWidget(this);
#endif

    server = new ServerSettings(widget);
    playback = new PlaybackSettings(widget);
    output = new OutputSettings(widget);
    interface = new InterfaceSettings(widget);
    lyrics = new LyricSettings(widget);
    server->load();
    playback->load();
    output->load();
    interface->load();
    const QList<UltimateLyricsProvider *> &lprov=lp->getProviders();
    lyrics->Load(lprov);
//     if (lprov.isEmpty()) {
//         connect(lp, SIGNAL(providersUpdated(const QList<SongInfoProvider *> &)), lyrics,
//                 SLOT(Load(providersUpdated(const QList<SongInfoProvider *> &))));
//     }
#ifdef ENABLE_KDE_SUPPORT
    KPageWidgetItem *page=widget->addPage(server, i18n("Server"));
    page->setHeader(i18n("MPD Backend Settings"));
    page->setIcon(KIcon("server-database"));
    page=widget->addPage(playback, i18n("Playback"));
    page->setHeader(i18n("Playback Settings"));
    page->setIcon(KIcon("media-playback-start"));
    page=widget->addPage(output, i18n("Output"));
    page->setHeader(i18n("Control Active Outputs"));
    page->setIcon(KIcon("speaker"));
    page=widget->addPage(interface, i18n("Interface"));
    page->setHeader(i18n("Interface Settings"));
    page->setIcon(KIcon("preferences-desktop-color"));
    page=widget->addPage(lyrics, i18n("Lyrics"));
    page->setHeader(i18n("Lyrics Settings"));
    page->setIcon(KIcon("view-media-lyrics"));
    setMainWidget(widget);

    setButtons(KDialog::Ok | KDialog::Apply | KDialog::Cancel);
    setCaption(i18n("Configure"));
#else
    widget->AddTab(new ConfigPage(this, tr("MPD Backend Settings"), QIcon::fromTheme("server-database"), server),
                   QIcon::fromTheme("server-database"), tr("Server"));
    widget->AddTab(new ConfigPage(this, tr("Playback Settings"), QIcon::fromTheme("media-playback-start"), playback),
                   QIcon::fromTheme("media-playback-start"), tr("Playback"));
    widget->AddTab(new ConfigPage(this, tr("Control Active Outputs"), QIcon::fromTheme("speaker"), output),
                   QIcon::fromTheme("speaker"), tr("Output"));
    widget->AddTab(new ConfigPage(this, tr("Interface Settings"), QIcon::fromTheme("preferences-desktop-color"), interface),
                   QIcon::fromTheme("preferences-desktop-color"), tr("Interface"));
    widget->AddTab(new ConfigPage(this, tr("Lyrics Settings"), QIcon::fromTheme("view-media-lyrics"), lyrics),
                   QIcon::fromTheme("view-media-lyrics"), tr("Lyrics"));
    proxy = new ProxySettings(this);
    proxy->load();
    widget->AddTab(new ConfigPage(this, tr("Proxy Settings"), QIcon::fromTheme("preferences-system-network"), proxy),
                   QIcon::fromTheme("preferences-system-network"), tr("Proxy"));
    widget->SetMode(FancyTabWidget::Mode_LargeSidebar);
    setWindowTitle(tr("Configure"));

    QBoxLayout *layout=new QBoxLayout(QBoxLayout::TopToBottom, this);
    layout->addWidget(widget);
    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel|QDialogButtonBox::Apply,
                                     Qt::Horizontal, this);
    layout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(clicked(QAbstractButton *)), this, SLOT(buttonPressed(QAbstractButton *)));
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
#endif
    resize(600, 400);
}

void PreferencesDialog::writeSettings()
{
    server->save();
    playback->save();
    output->save();
    interface->save();
#ifndef ENABLE_KDE_SUPPORT
    proxy->save();
#endif
    Settings::self()->saveLyricProviders(lyrics->EnabledProviders());
    emit settingsSaved();
}

#ifdef ENABLE_KDE_SUPPORT
void PreferencesDialog::slotButtonClicked(int button)
{
    switch (button) {
    case KDialog::Ok:
    case KDialog::Apply:
        writeSettings();
        break;
    case KDialog::Cancel:
        reject();
        break;
    default:
        // unhandled button
        break;
    }

    if (button == KDialog::Ok) {
        accept();
    }

    KDialog::slotButtonClicked(button);
}
#endif

/**
 * TODO:MERGE it all with the kde version... (submission part..)
 */
#ifndef ENABLE_KDE_SUPPORT
void PreferencesDialog::buttonPressed(QAbstractButton *button)
{
    switch (buttonBox->buttonRole(button)) {
    case QDialogButtonBox::AcceptRole:
    case QDialogButtonBox::ApplyRole:
        writeSettings();
        break;
    case QDialogButtonBox::RejectRole:
        break;
    default:
        // unhandled button
        break;
    }
}
#endif
