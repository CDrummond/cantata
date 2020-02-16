/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "serversettings.h"
#include "settings.h"
#include "support/inputdialog.h"
#include "support/messagebox.h"
#include "widgets/icons.h"
#include "db/mpdlibrarydb.h"
#ifdef ENABLE_SIMPLE_MPD_SUPPORT
#include "mpd-interface/mpduser.h"
#endif
#include "support/utils.h"
#ifdef AVAHI_FOUND
#include "findmpddialog.h"
#endif
#include <QDir>
#include <QComboBox>
#include <QPushButton>
#include <QValidator>
#include <QStyle>
#include <QStandardPaths>
#include <algorithm>

#define REMOVE(w) \
    w->setVisible(false); \
    w->deleteLater(); \
    w=0;

class CollectionNameValidator : public QValidator
{
    public:

    CollectionNameValidator(QObject *parent) : QValidator(parent) { }

    State validate(QString &input, int &) const override
    {
        #ifdef ENABLE_SIMPLE_MPD_SUPPORT
        if (input.startsWith(MPDUser::constName)) {
            return Invalid;
        }
        #endif
        if (-1!=input.indexOf("/") || -1!=input.indexOf("\\") || -1!=input.indexOf("*") || -1!=input.indexOf("?")) {
            return Invalid;
        }
        return Acceptable;
    }
};

ServerSettings::ServerSettings(QWidget *p)
    : QWidget(p)
    , haveBasicCollection(false)
    , isCurrentConnection(false)
    , allOptions(true) // will be toggled
    , prevIndex(0)
{
    setupUi(this);
    #if defined ENABLE_DEVICES_SUPPORT
    musicFolderNoteLabel->appendText(QLatin1String("<i> ")+tr("This folder will also be used to locate music files "
                                     "for tag-editing, replay gain, and transferring to (and from) devices.")+QLatin1String("</i>"));
    #else
    musicFolderNoteLabel->appendText(QLatin1String("<i> ")+tr("This folder will also be used to locate music files "
                                     "for tag-editing, replay gain, etc.")+QLatin1String("</i>"));
    #endif
    musicFolderNoteLabel->appendText(QLatin1String("<br/><i><a href=\"https://github.com/CDrummond/cantata/wiki/Accessing-music-files,-and-covers\">")+
                                     tr("More information")+QLatin1String("</a></i>"));

    connect(combo, SIGNAL(activated(int)), SLOT(showDetails(int)));
    connect(removeButton, SIGNAL(clicked(bool)), SLOT(remove()));
    connect(addButton, SIGNAL(clicked(bool)), SLOT(add()));
    connect(name, SIGNAL(textChanged(QString)), SLOT(nameChanged()));
    connect(basicDir, SIGNAL(textChanged(QString)), SLOT(basicDirChanged()));
    addButton->setIcon(Icons::self()->addIcon);
    removeButton->setIcon(Icons::self()->minusIcon);
    addButton->setAutoRaise(true);
    removeButton->setAutoRaise(true);

    #if defined Q_OS_WIN
    hostLabel->setText(tr("Host:"));
    #endif

    name->setValidator(new CollectionNameValidator(this));
    #ifndef ENABLE_HTTP_STREAM_PLAYBACK
    REMOVE(streamUrlLabel)
    REMOVE(streamUrl)
    REMOVE(streamUrlNoteLabel)
    #endif
    #ifdef ENABLE_HTTP_SERVER
    allowLocalStreamingLabel->setToolTip(allowLocalStreaming->toolTip());
    #else
    REMOVE(allowLocalStreaming)
    REMOVE(allowLocalStreamingLabel)
    REMOVE(allowLocalStreamingNoteLabel)
    #endif
    autoUpdate->setToolTip(autoUpdate->toolTip());

    #ifdef Q_OS_MAC
    expandingSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
    #endif

    #ifdef AVAHI_FOUND
    discoveryButton = new QPushButton(tr("Discover..."), this);
    hostLayout->insertWidget(hostLayout->count(), discoveryButton);
    connect(discoveryButton, &QPushButton::clicked, this, &ServerSettings::detectMPDs);
    #endif
}

void ServerSettings::load()
{
    QList<MPDConnectionDetails> all=Settings::self()->allConnections();
    QString currentCon=Settings::self()->currentConnection();

    std::sort(all.begin(), all.end());
    combo->clear();
    int idx=0;
    haveBasicCollection=false;
    for (MPDConnectionDetails d: all) {
        combo->addItem(d.getName(), d.name);
        if (d.name==currentCon) {
            prevIndex=idx;
        }
        idx++;
        #ifdef ENABLE_SIMPLE_MPD_SUPPORT
        if (d.name==MPDUser::constName) {
            d.dir=MPDUser::self()->details().dir;
            haveBasicCollection=true;
            prevBasic=d;
        }
        #endif
        DeviceOptions opts;
        opts.load(MPDConnectionDetails::configGroupName(d.name), true);
        collections.append(Collection(d, opts));
    }
    combo->setCurrentIndex(prevIndex);
    setDetails(collections.at(prevIndex).details);
    removeButton->setEnabled(combo->count()>1);
}

void ServerSettings::save()
{
    if (combo->count()<1) {
        return;
    }

    Collection current=collections.at(combo->currentIndex());
    current.details=getDetails();
    collections.replace(combo->currentIndex(), current);

    QList<MPDConnectionDetails> existingInConfig=Settings::self()->allConnections();
    QList<Collection> toAdd;

    for (const Collection &c: collections) {
        bool found=false;
        for (int i=0; i<existingInConfig.count(); ++i) {
            MPDConnectionDetails e=existingInConfig.at(i);
            if (e.name==c.details.name) {
                existingInConfig.removeAt(i);
                found=true;
                if (c.details.hostname!=e.hostname || c.details.port!=e.port || c.details.password!=e.password ||
                    c.details.dir!=e.dir
                    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
                    || c.details.streamUrl!=e.streamUrl
                    #endif
                    || c.details.allowLocalStreaming!=e.allowLocalStreaming
                    || c.details.autoUpdate!=e.autoUpdate
                    ) {
                    toAdd.append(c);
                }
            }
        }
        if (!found) {
            toAdd.append(c);
        }
        #ifdef ENABLE_SIMPLE_MPD_SUPPORT
        if (c.details.name==MPDUser::constName) {
            prevBasic=c;
        }
        #endif
    }

    for (const MPDConnectionDetails &c: existingInConfig) {
        Settings::self()->removeConnectionDetails(c.name);
    }

    for (const Collection &c: toAdd) {
        Settings::self()->saveConnectionDetails(c.details);
        c.namingOpts.save(MPDConnectionDetails::configGroupName(c.details.name), true);
    }

    #ifdef ENABLE_SIMPLE_MPD_SUPPORT
    if (!haveBasicCollection && MPDUser::self()->isSupported()) {
        MPDUser::self()->cleanup();
    }

    if (current.details.name==MPDUser::constName) {
        MPDUser::self()->setDetails(current.details);
    }
    #endif
    Settings::self()->saveCurrentConnection(current.details.name);
    MpdLibraryDb::removeUnusedDbs();
}

void ServerSettings::cancel()
{
    #ifdef ENABLE_SIMPLE_MPD_SUPPORT
    // If we are canceling any changes, then we need to restore user settings...
    if (prevBasic.details.name==MPDUser::constName) {
        MPDUser::self()->setDetails(prevBasic.details);
    }
    #endif
}

void ServerSettings::showDetails(int index)
{
    if (-1!=prevIndex && index!=prevIndex) {
        MPDConnectionDetails details=getDetails();
        if (details.name.isEmpty()) {
            details.name=generateName(prevIndex);
        }
        collections.replace(prevIndex, details);
        #ifdef ENABLE_SIMPLE_MPD_SUPPORT
        if (details.name!=MPDUser::constName)
        #endif
        {
            combo->setItemText(prevIndex, details.name);
        }
    }
    setDetails(collections.at(index).details);
    prevIndex=index;
}

void ServerSettings::add()
{
    #ifdef ENABLE_SIMPLE_MPD_SUPPORT
    bool addStandard=true;

    if (!haveBasicCollection && MPDUser::self()->isSupported()) {
        static const QChar constBullet(0x2022);

        switch (MessageBox::questionYesNoCancel(this,
                                   QLatin1String("<p>")+
                                   tr("Which type of collection do you wish to connect to?")+QLatin1String("<br/><br/>")+
                                   constBullet+QLatin1Char(' ')+tr("Standard - music collection may be shared, is on another machine, is "
                                                                     "already setup, or you wish to enable access from other clients (e.g. "
                                                                     "MPDroid)")+QLatin1String("<br/><br/>")+
                                   constBullet+QLatin1Char(' ')+tr("Basic - music collection is not shared with others, and Cantata will "
                                                                     "configure and control the MPD instance. This setup will be exclusive "
                                                                     "to Cantata, and will <b>not</b> be accessible to other MPD clients.")+
                                                                     QLatin1String("<br/><br/>")+
                                   tr("<i><b>NOTE:</b> %1</i>").arg(tr("If you wish to have an advanced MPD setup (e.g. multiple audio "
                                        "outputs, full DSD support, etc) then you <b>must</b> choose 'Standard'")),
                                   tr("Add Collection"), GuiItem(tr("Standard")), GuiItem(tr("Basic")))) {
        case MessageBox::Yes: addStandard=true; break;
        case MessageBox::No: addStandard=false; break;
        default: return;
        }
    }
    #endif
    MPDConnectionDetails details;
    #ifdef ENABLE_SIMPLE_MPD_SUPPORT
    if (addStandard) {
    #endif
        details.name=generateName();
        details.port=6600;
        details.hostname=QLatin1String("localhost");
        details.dir=QLatin1String("/var/lib/mpd/music/");
        details.allowLocalStreaming=true;
        details.autoUpdate=false;
        combo->addItem(details.name);
    #ifdef ENABLE_SIMPLE_MPD_SUPPORT
    } else {
        details=MPDUser::self()->details(true);
        QString dir=QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
        if (dir.isEmpty()) {
            QString dir=QDir::homePath()+"/Music";
            dir=dir.replace("//", "/");
        }
        dir=Utils::fixPath(dir);
        basicDir->setText(dir);
        MPDUser::self()->setMusicFolder(dir);
        combo->addItem(MPDUser::translatedName());
    }
    #endif
    removeButton->setEnabled(combo->count()>1);
    collections.append(Collection(details));
    combo->setCurrentIndex(combo->count()-1);
    prevIndex=combo->currentIndex();
    setDetails(details);
}

void ServerSettings::remove()
{
    int index=combo->currentIndex();
    #ifdef ENABLE_SIMPLE_MPD_SUPPORT
    QString cName=1==stackedWidget->currentIndex() ? MPDUser::translatedName() : name->text();
    #else
    QString cName=name->text();
    #endif
    if (combo->count()>1 && MessageBox::Yes==MessageBox::questionYesNo(this, tr("Delete '%1'?").arg(cName),
                                                                       tr("Delete"), StdGuiItem::del(), StdGuiItem::cancel())) {
        bool isLast=index==(combo->count()-1);
        combo->removeItem(index);
        combo->setCurrentIndex(isLast ? index-1 : index);
        prevIndex=combo->currentIndex();
        collections.removeAt(index);
        if (1==stackedWidget->currentIndex()) {
            haveBasicCollection=false;
        }
        setDetails(collections.at(combo->currentIndex()).details);
    }
    removeButton->setEnabled(combo->count()>1);
}

void ServerSettings::nameChanged()
{
    combo->setItemText(combo->currentIndex(), name->text().trimmed());
}

void ServerSettings::basicDirChanged()
{
    if (!prevBasic.details.dir.isEmpty()) {
        QString d=Utils::fixPath(basicDir->text().trimmed());
        basicMusicFolderNoteLabel->setOn(d.isEmpty() || d!=prevBasic.details.dir);
    }
}

#ifdef AVAHI_FOUND
void ServerSettings::adoptServerSettings(QString ip, QString p)
{
    host->setText(ip);
    port->setValue(p.toInt());
}

void ServerSettings::detectMPDs()
{
    FindMpdDialog findMpdDlg(this);
    QObject::connect(&findMpdDlg, &FindMpdDialog::serverChosen, this, &ServerSettings::adoptServerSettings);
    findMpdDlg.exec();
}
#endif

QString ServerSettings::generateName(int ignore) const
{
    QString n;
    QSet<QString> collectionNames;
    for (int i=0; i<collections.size(); ++i) {
        if (i!=ignore) {
            collectionNames.insert(collections.at(i).details.name);
        }
    }

    for (int i=1; i<512; ++i) {
        n=tr("New Collection %1").arg(i);
        if (!collectionNames.contains(n)) {
            break;
        }
    }

    return n;
}

void ServerSettings::setDetails(const MPDConnectionDetails &details)
{
    #ifdef ENABLE_SIMPLE_MPD_SUPPORT
    if (details.name==MPDUser::constName) {
        basicDir->setText(Utils::convertPathForDisplay(details.dir));
        stackedWidget->setCurrentIndex(1);
    } else {
    #endif
        name->setText(details.name.isEmpty() ? tr("Default") : details.name);
        host->setText(details.hostname);
        port->setValue(details.port);
        password->setText(details.password);
        dir->setText(Utils::convertPathForDisplay(details.dir));
        #ifdef ENABLE_HTTP_STREAM_PLAYBACK
        streamUrl->setText(details.streamUrl);
        #endif
        #ifdef ENABLE_HTTP_SERVER
        allowLocalStreaming->setChecked(details.allowLocalStreaming);
        #endif
        autoUpdate->setChecked(details.autoUpdate);
        stackedWidget->setCurrentIndex(0);
    #ifdef ENABLE_SIMPLE_MPD_SUPPORT
    }
    #endif
}

MPDConnectionDetails ServerSettings::getDetails() const
{
    MPDConnectionDetails details;
    if (0==stackedWidget->currentIndex()) {
        details.name=name->text().trimmed();
        #ifdef ENABLE_SIMPLE_MPD_SUPPORT
        if (details.name==MPDUser::constName) {
            details.name=QString();
        }
        #endif
        details.hostname=host->text().trimmed();
        details.port=port->value();
        details.password=password->text();
        details.dir=Utils::convertPathFromDisplay(dir->text());
        #ifdef ENABLE_HTTP_STREAM_PLAYBACK
        details.streamUrl=streamUrl->text().trimmed();
        #endif
        #ifdef ENABLE_HTTP_SERVER
        details.allowLocalStreaming=allowLocalStreaming->isChecked();
        #endif
        details.autoUpdate=autoUpdate->isChecked();
    }
    #ifdef ENABLE_SIMPLE_MPD_SUPPORT
    else {
        details=MPDUser::self()->details(true);
        details.dir=Utils::convertPathFromDisplay(basicDir->text());
        MPDUser::self()->setMusicFolder(details.dir);
    }
    #endif
    details.setDirReadable();
    return details;
}

#include "moc_serversettings.cpp"
