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

#include "serversettings.h"
#include "settings.h"
#include "localize.h"
#include "inputdialog.h"
#include "messagebox.h"
#include "icons.h"
#include "musiclibrarymodel.h"
#include "mpduser.h"
#include <QDir>
#include <QComboBox>
#include <QPushButton>
#include <QValidator>
#include <QStyle>

#define REMOVE(w) \
    w->setVisible(false); \
    w->deleteLater(); \
    w=0;

class CoverNameValidator : public QValidator
{
    public:

    CoverNameValidator(QObject *parent) : QValidator(parent) { }

    State validate(QString &input, int &) const
    {
        for (int i=0; i<input.length(); ++i) {
            if (!input[i].isLetterOrNumber() && '%'!=input[i] && ' '!=input[i] && '-'!=input[i]) {
                return Invalid;
            }
        }

        return Acceptable;
    }
};

class CollectionNameValidator : public QValidator
{
    public:

    CollectionNameValidator(QObject *parent) : QValidator(parent) { }

    State validate(QString &input, int &) const
    {
        return input.startsWith(MPDUser::constName) ? Invalid : Acceptable;
    }
};

ServerSettings::ServerSettings(QWidget *p)
    : QWidget(p)
    , haveBasicCollection(false)
    , prevIndex(0)
{
    setupUi(this);
    #if defined ENABLE_DEVICES_SUPPORT
    musicFolderNoteLabel->setText(musicFolderNoteLabel->text()+
                                  i18n("<i> This folder will also be used to locate music files "
                                       "for transferring to (and from) devices.</i>"));
    #endif
    connect(combo, SIGNAL(activated(int)), SLOT(showDetails(int)));
    connect(removeButton, SIGNAL(clicked(bool)), SLOT(remove()));
    connect(addButton, SIGNAL(clicked(bool)), SLOT(add()));
    connect(name, SIGNAL(textChanged(QString)), SLOT(nameChanged()));
    connect(basicDir, SIGNAL(textChanged(QString)), SLOT(basicDirChanged()));
    addButton->setIcon(Icon("list-add"));
    removeButton->setIcon(Icon("list-remove"));
    addButton->setAutoRaise(true);
    removeButton->setAutoRaise(true);

    dynamizerPort->setSpecialValueText(i18n("Not used"));
    dynamizerPort->setRange(0, 65535);
    #if defined Q_OS_WIN
    hostLabel->setText(i18n("Host:"));
    socketNoteLabel->setVisible(false);
    dynamizerNoteLabel->setText(i18nc("Qt-only, windows",
                                      "<i><b>NOTE:</b> 'Dynamizer port' is only relevant if "
                                      "you wish to make use of 'dynamic playlists'. In order to function, the <code>"
                                      "cantata-dynamic</code> application <b>must</b> already have been installed, "
                                      "and started, on the relevant host - Cantata itself cannot control the "
                                      "starting/stopping of this service.</i>"));
    #else
    dynamizerNoteLabel->setText(i18n("<i><b>NOTE:</b> 'Dynamizer port' is only relevant if you wish to use a "
                                     "system-wide, or non-local, instance of the Cantata dynamizer. "
                                     "For this to function, the <code>"
                                     "cantata-dynamic</code> application <b>must</b> already have been installed, "
                                     "and started, on the relevant host - Cantata itself cannot control the "
                                     "starting/stopping of this service. If this is not set, then Cantata will "
                                     "use a per-user instance of the dynamizer to facilitate dynamic playlists.</i>"));
    #endif

    coverName->setToolTip(i18n("<p>Filename (without extension) to save downloaded covers as.<br/>If left blank 'cover' will be used.<br/><br/>"
                               "<i>%artist% will be replaced with album artist of the current song, and %album% will be replaced with the album name.</i></p>"));
    basicCoverName->setToolTip(coverName->toolTip());
    basicCoverNameLabel->setToolTip(coverName->toolTip());
    coverNameLabel->setToolTip(coverName->toolTip());
    coverName->setValidator(new CoverNameValidator(this));
    basicCoverName->setValidator(new CoverNameValidator(this));
    name->setValidator(new CollectionNameValidator(this));
    #ifndef ENABLE_HTTP_STREAM_PLAYBACK
    REMOVE(streamUrlLabel)
    REMOVE(streamUrl)
    REMOVE(streamUrlNoteLabel)
    #endif
}

void ServerSettings::load()
{
    QList<MPDConnectionDetails> all=Settings::self()->allConnections();
    QString currentCon=Settings::self()->currentConnection();

    qSort(all);
    combo->clear();
    int idx=0;
    haveBasicCollection=false;
    foreach (MPDConnectionDetails d, all) {
        combo->addItem(d.getName(), d.name);
        if (d.name==currentCon) {
            prevIndex=idx;
        }
        idx++;
        if (d.name==MPDUser::constName) {
            d.dir=MPDUser::self()->details().dir;
            haveBasicCollection=true;
            prevBasic=d;
        }
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

    foreach (const Collection &c, collections) {
        bool found=false;
        for (int i=0; i<existingInConfig.count(); ++i) {
            MPDConnectionDetails e=existingInConfig.at(i);
            if (e.name==c.details.name) {
                existingInConfig.removeAt(i);
                found=true;
                if (c.details.hostname!=e.hostname || c.details.port!=e.port || c.details.password!=e.password ||
                    c.details.dir!=e.dir || c.details.dynamizerPort!=e.dynamizerPort || c.details.coverName!=e.coverName
                    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
                    || c.details.streamUrl!=e.streamUrl
                    #endif
                    ) {
                    toAdd.append(c);
                }
            }
        }
        if (!found) {
            toAdd.append(c);
        }
        if (c.details.name==MPDUser::constName) {
            prevBasic=c;
        }
    }

    foreach (const MPDConnectionDetails &c, existingInConfig) {
        Settings::self()->removeConnectionDetails(c.name);
    }

    foreach (const Collection &c, toAdd) {
        Settings::self()->saveConnectionDetails(c.details);
        c.namingOpts.save(MPDConnectionDetails::configGroupName(c.details.name), true);
    }

    if (!haveBasicCollection && MPDUser::self()->isSupported()) {
        MPDUser::self()->cleanup();
    }

    if (current.details.name==MPDUser::constName) {
        MPDUser::self()->setDetails(current.details);
    }
    Settings::self()->saveCurrentConnection(current.details.name);
}

void ServerSettings::cancel()
{
    // If we are canceling any changes, then we need to restore user settings...
    if (prevBasic.details.name==MPDUser::constName) {
        MPDUser::self()->setDetails(prevBasic.details);
    }
}

void ServerSettings::showDetails(int index)
{
    if (-1!=prevIndex && index!=prevIndex) {
        MPDConnectionDetails details=getDetails();
        if (details.name.isEmpty()) {
            details.name=generateName(prevIndex);
        }
        collections.replace(prevIndex, details);
        if (details.name!=MPDUser::constName) {
            combo->setItemText(prevIndex, details.name);
        }
    }
    setDetails(collections.at(index).details);
    prevIndex=index;
}

void ServerSettings::add()
{
    bool addStandard=true;

    if (!haveBasicCollection && MPDUser::self()->isSupported()) {
        switch (MessageBox::questionYesNoCancel(this,
                                   i18n("Which type of collection do you wish to connect to?<br/><ul>"
                                   "<li>Standard - music collection may be shared, is on another machine, or is already setup</li>"
                                   "<li>Basic - music collection is not shared with others, and Cantata will configure and control the MPD instance</li></ul>"),
                                   i18n("Add Collection"), GuiItem(i18n("Standard")), GuiItem(i18n("Basic")))) {
        case MessageBox::Yes: addStandard=true; break;
        case MessageBox::No: addStandard=false; break;
        default: return;
        }
    }

    MPDConnectionDetails details;
    if (addStandard) {
        details.name=generateName();
        details.port=6600;
        details.hostname=QLatin1String("localhost");
        details.dir=QLatin1String("/var/lib/mpd/music");
        details.dynamizerPort=0;
        combo->addItem(details.name);
    } else {
        details=MPDUser::self()->details(true);
        basicDir->setText(details.dir);
        combo->addItem(MPDUser::translatedName());
    }
    removeButton->setEnabled(combo->count()>1);
    collections.append(Collection(details));
    combo->setCurrentIndex(combo->count()-1);
    prevIndex=combo->currentIndex();
    setDetails(details);
}

void ServerSettings::remove()
{
    int index=combo->currentIndex();
    QString cName=1==stackedWidget->currentIndex() ? MPDUser::translatedName() : name->text();
    if (combo->count()>1 && MessageBox::Yes==MessageBox::questionYesNo(this, i18n("Delete <b>%1</b>?", cName),
                                                                       i18n("Delete"), StdGuiItem::del(), StdGuiItem::cancel())) {
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
        QString d=basicDir->text().trimmed();
        basicMusicFolderNoteLabel->setOn(d.isEmpty() || d!=prevBasic.details.dir);
    }
}

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
        n=i18n("New Collection %1", i);
        if (!collectionNames.contains(n)) {
            break;
        }
    }

    return n;
}

void ServerSettings::setDetails(const MPDConnectionDetails &details)
{
    if (details.name==MPDUser::constName) {
        basicDir->setText(details.dir);
        basicCoverName->setText(details.coverName);
        stackedWidget->setCurrentIndex(1);
    } else {
        name->setText(details.name.isEmpty() ? i18n("Default") : details.name);
        host->setText(details.hostname);
        port->setValue(details.port);
        password->setText(details.password);
        dir->setText(details.dir);
        dynamizerPort->setValue(details.dynamizerPort);
        coverName->setText(details.coverName);
        #ifdef ENABLE_HTTP_STREAM_PLAYBACK
        streamUrl->setText(details.streamUrl);
        #endif
        stackedWidget->setCurrentIndex(0);
    }
}

MPDConnectionDetails ServerSettings::getDetails() const
{
    MPDConnectionDetails details;
    if (0==stackedWidget->currentIndex()) {
        details.name=name->text().trimmed();
        if (details.name==MPDUser::constName) {
            details.name=QString();
        }
        details.hostname=host->text().trimmed();
        details.port=port->value();
        details.password=password->text();
        details.dir=dir->text().trimmed();
        details.dynamizerPort=dynamizerPort->value();
        details.coverName=coverName->text().trimmed();
        #ifdef ENABLE_HTTP_STREAM_PLAYBACK
        details.streamUrl=streamUrl->text().trimmed();
        #endif
    } else {
        details=MPDUser::self()->details(true);
        details.dir=basicDir->text().trimmed();
        details.coverName=basicCoverName->text().trimmed();
        MPDUser::self()->setMusicFolder(details.dir);
    }
    details.dirReadable=details.dir.isEmpty() ? false : QDir(details.dir).isReadable();
    return details;
}
