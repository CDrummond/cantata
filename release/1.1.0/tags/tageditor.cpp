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

#include "tageditor.h"
#include "tags.h"
#include "musiclibrarymodel.h"
#include "mpdconnection.h"
#include "settings.h"
#include "messagebox.h"
#include "inputdialog.h"
#include "localize.h"
#include "trackorganiser.h"
#include "cuefile.h"
#ifdef ENABLE_DEVICES_SUPPORT
#include "devicesmodel.h"
#include "device.h"
#endif
#include <QMenu>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QEventLoop>
#include <QDir>

static bool equalTags(const Song &a, const Song &b, bool compareCommon)
{
    return (compareCommon || a.track==b.track) && a.year==b.year && a.disc==b.disc &&
           a.artist==b.artist && a.genre==b.genre && a.album==b.album &&
           a.albumartist==b.albumartist && (compareCommon || a.title==b.title);
}

static void setString(QString &str, const QString &v, bool skipEmpty) {
    if (!skipEmpty || !v.isEmpty()) {
        str=v;
    }
}

static int iCount=0;

int TagEditor::instanceCount()
{
    return iCount;
}

TagEditor::TagEditor(QWidget *parent, const QList<Song> &songs,
                     const QSet<QString> &existingArtists, const QSet<QString> &existingAlbumArtists,
                     const QSet<QString> &existingAlbums, const QSet<QString> &existingGenres, const QString &udi)
    : Dialog(parent, "TagEditor", QSize(500, 200))
    #ifdef ENABLE_DEVICES_SUPPORT
    , deviceUdi(udi)
    #endif
    , currentSongIndex(-1)
    , updating(false)
    , haveArtists(false)
    , haveAlbumArtists(false)
    , haveAlbums(false)
    , haveGenres(false)
    , saving(false)
{
    iCount++;
    foreach (const Song &s, songs) {
        if (CueFile::isCue(s.file)) {
            continue;
        }
        if (s.guessed) {
            Song song(s);
            song.revertGuessedTags();
            original.append(song);
        } else {
            original.append(s);
        }
    }

    if (original.isEmpty()) {
        deleteLater();
        return;
    }

    #ifdef ENABLE_DEVICES_SUPPORT
    if (deviceUdi.isEmpty()) {
        baseDir=MPDConnection::self()->getDetails().dir;
    } else {
        Device *dev=getDevice(udi, parentWidget());

        if (!dev) {
            deleteLater();
            return;
        }

        baseDir=dev->path();
    }
    #else
    Q_UNUSED(udi)
    baseDir=MPDConnection::self()->getDetails().dir;
    #endif
    qSort(original);

    QWidget *mainWidet = new QWidget(this);
    setupUi(mainWidet);
    track->setAllowEmpty();
    disc->setAllowEmpty();
    year->setAllowEmpty();
    setMainWidget(mainWidet);
    ButtonCodes buttons=Ok|Cancel|Reset|User3;
    if (songs.count()>1) {
        buttons|=User2|User1;
    }
    setButtons(buttons);
    setCaption(i18n("Tags"));
    progress->setVisible(false);
    if (songs.count()>1) {
        setButtonGuiItem(User2, StdGuiItem::back(true));
        setButtonGuiItem(User1,StdGuiItem::forward(true));
        enableButton(User1, false);
        enableButton(User2, false);
    }
    setButtonGuiItem(Ok, StdGuiItem::save());
    setButtonGuiItem(User3, GuiItem(i18n("Tools"), "tools-wizard"));
    QMenu *toolsMenu=new QMenu(this);
    toolsMenu->addAction(i18n("Apply \"Various Artists\" Workaround"), this, SLOT(applyVa()));
    toolsMenu->addAction(i18n("Revert \"Various Artists\" Workaround"), this, SLOT(revertVa()));
    toolsMenu->addAction(i18n("Set 'Album Artist' from 'Artist'"), this, SLOT(setAlbumArtistFromArtist()));
    toolsMenu->addAction(i18n("Capitalize"), this, SLOT(capitalise()));
    toolsMenu->addAction(i18n("Adjust Track Numbers"), this, SLOT(adjustTrackNumbers()));
    setButtonMenu(User3, toolsMenu, InstantPopup);
    enableButton(Ok, false);
    enableButton(Reset, false);

    setAttribute(Qt::WA_DeleteOnClose);

    QStringList strings=existingArtists.toList();
    strings.sort();
    artist->clear();
    artist->insertItems(0, strings);

    strings=existingAlbumArtists.toList();
    strings.sort();
    albumArtist->clear();
    albumArtist->insertItems(0, strings);

    strings=existingAlbums.toList();
    strings.sort();
    album->clear();
    album->insertItems(0, strings);

    strings=existingGenres.toList();
    strings.sort();
    genre->clear();
    genre->insertItems(0, strings);

    trackName->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    trackName->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLength);
    trackName->view()->setTextElideMode(Qt::ElideLeft);

    if (original.count()>1) {
        QSet<QString> songArtists;
        QSet<QString> songAlbumArtists;
        QSet<QString> songAlbums;
        QSet<QString> songGenres;
        QSet<int> songYears;
        QSet<int> songDiscs;

        foreach (const Song &s, original) {
            if (!s.artist.isEmpty()) {
                songArtists.insert(s.artist);
            }
            if (!s.albumartist.isEmpty()) {
                songAlbumArtists.insert(s.albumartist);
            }
            if (!s.album.isEmpty()) {
                songAlbums.insert(s.album);
            }
            if (!s.genre.isEmpty()) {
                songGenres.insert(s.genre);
            }
            songYears.insert(s.year);
            songDiscs.insert(s.disc);
            if (songArtists.count()>1 && songAlbumArtists.count()>1 && songAlbums.count()>1 && songGenres.count()>1 && songYears.count()>1 && songDiscs.count()>1) {
                break;
            }
        }
        Song all;
        all.file.clear();
        all.title.clear();
        all.track=0;
        all.artist=1==songArtists.count() ? *(songArtists.begin()) : QString();
        all.albumartist=1==songAlbumArtists.count() ? *(songAlbumArtists.begin()) : QString();
        all.album=1==songAlbums.count() ? *(songAlbums.begin()) : QString();
        all.genre=1==songGenres.count() ? *(songGenres.begin()) : QString();
        all.year=1==songYears.count() ? *(songYears.begin()) : 0;
        all.disc=1==songDiscs.count() ? *(songDiscs.begin()) : 0;
        original.prepend(all);
        artist->setFocus();
        haveArtists=!songArtists.isEmpty();
        haveAlbumArtists=!songAlbumArtists.isEmpty();
        haveAlbums=!songAlbums.isEmpty();
        haveGenres=!songGenres.isEmpty();
    } else {
        title->setFocus();
    }
    edited=original;
    setIndex(0);
    bool first=original.count()>1;
    foreach (const Song &s, original) {
        if (first) {
            trackName->insertItem(trackName->count(), i18n("All tracks"));
            first=false;
        } else {
            trackName->insertItem(trackName->count(), s.file);
        }
    }
    connect(title, SIGNAL(textChanged(const QString &)), SLOT(checkChanged()));
    connect(artist, SIGNAL(activated(int)), SLOT(checkChanged()));
    connect(artist, SIGNAL(editTextChanged(const QString &)), SLOT(checkChanged()));
    connect(albumArtist, SIGNAL(activated(int)), SLOT(checkChanged()));
    connect(albumArtist, SIGNAL(editTextChanged(const QString &)), SLOT(checkChanged()));
    connect(album, SIGNAL(activated(int)), SLOT(checkChanged()));
    connect(album, SIGNAL(editTextChanged(const QString &)), SLOT(checkChanged()));
    connect(genre, SIGNAL(activated(int)), SLOT(checkChanged()));
    connect(track, SIGNAL(valueChanged(int)), SLOT(checkChanged()));
    connect(disc, SIGNAL(valueChanged(int)), SLOT(checkChanged()));
    connect(genre, SIGNAL(editTextChanged(const QString &)), SLOT(checkChanged()));
    connect(year, SIGNAL(valueChanged(int)), SLOT(checkChanged()));
    connect(trackName, SIGNAL(activated(int)), SLOT(setIndex(int)));
    connect(this, SIGNAL(update()), MPDConnection::self(), SLOT(update()));

    if (original.count()>0) {
        saveable=QDir(baseDir).isReadable();
    } else {
        saveable=false;
    }
}

TagEditor::~TagEditor()
{
    iCount--;
}

void TagEditor::fillSong(Song &s, bool isAll, bool skipEmpty) const
{
    Song all=original.at(0);
    bool haveAll=original.count()>1;

    if (!isAll) {
        setString(s.title, title->text().trimmed(), skipEmpty);
    }
    setString(s.artist, artist->text().trimmed(), skipEmpty && (!haveAll || all.artist.isEmpty()));
    setString(s.album, album->text().trimmed(), skipEmpty && (!haveAll || all.album.isEmpty()));
    setString(s.albumartist, albumArtist->text().trimmed(), skipEmpty && (!haveAll || all.albumartist.isEmpty()));
    if (!isAll) {
        s.track=track->value();
    }
    s.disc=disc->value();
    setString(s.genre, genre->text().trimmed(), skipEmpty && (!haveAll || all.genre.isEmpty()));
    s.year=year->value();
}

void TagEditor::setPlaceholderTexts()
{
    QString various=i18n("(Various)");

    if(0==currentSongIndex && original.count()>1) {
        Song all=original.at(0);
        artist->setPlaceholderText(all.artist.isEmpty() && haveArtists ? various : QString());
        album->setPlaceholderText(all.album.isEmpty() && haveAlbums ? various : QString());
        albumArtist->setPlaceholderText(all.albumartist.isEmpty() && haveAlbumArtists ? various : QString());
        genre->setPlaceholderText(all.genre.isEmpty() && haveGenres ? various : QString());
    }
}

void TagEditor::enableOkButton()
{
    if (!saveable) {
        return;
    }

    enableButton(Ok, (editedIndexes.count()>1) ||
                     (1==original.count() && 1==editedIndexes.count()) ||
                     (1==editedIndexes.count() && !editedIndexes.contains(0)) );
    enableButton(Reset, isButtonEnabled(Ok));
}

void TagEditor::setLabelStates()
{
    Song o=original.at(currentSongIndex);
    Song e=edited.at(currentSongIndex);
    bool isAll=0==currentSongIndex && original.count()>1;

    titleLabel->setOn(!isAll && o.title!=e.title);
    artistLabel->setOn(o.artist!=e.artist);
    albumArtistLabel->setOn(o.albumartist!=e.albumartist);
    albumLabel->setOn(o.album!=e.album);
    trackLabel->setOn(!isAll && o.track!=e.track);
    discLabel->setOn(o.disc!=e.disc);
    genreLabel->setOn(o.genre!=e.genre);
    yearLabel->setOn(o.year!=e.year);
}

void TagEditor::applyVa()
{
    bool isAll=0==currentSongIndex && original.count()>1;

    if (MessageBox::No==MessageBox::questionYesNo(this, (isAll ? i18n("Apply \"Various Artists\" workaround to <b>all</b> tracks?")
                                                               : i18n("Apply \"Various Artists\" workaround?"))+
                                                           QLatin1String("<br/><hr/><br/>")+
                                                           i18n("<i>This will set 'Album artist' and 'Artist' to "
                                                                "\"Various Artists\", and set 'Title' to "
                                                                "\"TrackArtist - TrackTitle\"</i>"), i18n("Apply \"Various Artists\" Workaround"),
                                                  StdGuiItem::apply(), StdGuiItem::cancel())) {
        return;
    }

    if (isAll) {
        updating=true;
        for (int i=0; i<edited.count(); ++i) {
            Song s=edited.at(i);
            if (s.fixVariousArtists()) {
                if (0==i && QLatin1String(" - ")==s.title) {
                    s.title=QString();
                }
                edited.replace(i, s);
                updateEditedStatus(i);
                if (i==currentSongIndex) {
                    setSong(s);
                }
            }
        }
        updating=false;
        setLabelStates();
        enableOkButton();
    } else {
        Song s=edited.at(currentSongIndex);
        if (s.fixVariousArtists()) {
            edited.replace(currentSongIndex, s);
            updateEditedStatus(currentSongIndex);
            setSong(s);
        }
    }
}

void TagEditor::revertVa()
{
    bool isAll=0==currentSongIndex && original.count()>1;

    if (MessageBox::No==MessageBox::questionYesNo(this, (isAll ? i18n("Revert \"Various Artists\" workaround on <b>all</b> tracks?")
                                                               : i18n("Revert \"Various Artists\" workaround"))+
                                                           QLatin1String("<br/><hr/><br/>")+
                                                           i18n("<i>Where the 'Album artist' is the same as 'Artist' "
                                                                "and the 'Title' is of the format \"TrackArtist - TrackTitle\", "
                                                                "'Artist' will be taken from 'Title' and 'Title' itself will be "
                                                                "set to just the title. e.g. <br/><br/>"
                                                                "If 'Title' is \"Wibble - Wobble\", then 'Artist' will be set to "
                                                                "\"Wibble\" and 'Title' will be set to \"Wobble\"</i>"), i18n("Revert \"Various Artists\" Workaround"),
                                                  GuiItem(i18n("Revert")), StdGuiItem::cancel())) {
        return;
    }

    if (isAll) {
        updating=true;
        QSet<QString> artists;
        for (int i=1; i<edited.count(); ++i) {
            Song s=edited.at(i);
            if (s.revertVariousArtists()) {
                artists.insert(s.artist);
                edited.replace(i, s);
                updateEditedStatus(i);
                if (i==currentSongIndex) {
                    setSong(s);
                }
            }
        }
        Song s=edited.at(0);
        s.artist=artists.count()!=1 ? QString() : *artists.constBegin();
        edited.replace(0, s);
        updateEditedStatus(0);
        setSong(s);
        updating=false;
        setLabelStates();
        enableOkButton();
    } else {
        Song s=edited.at(currentSongIndex);
        if (s.revertVariousArtists()) {
            edited.replace(currentSongIndex, s);
            updateEditedStatus(currentSongIndex);
            setSong(s);
            enableOkButton();
        }
    }
}

void TagEditor::setAlbumArtistFromArtist()
{
    bool isAll=0==currentSongIndex && original.count()>1;

    if (MessageBox::No==MessageBox::questionYesNo(this, isAll ? i18n("Set 'Album Artist' from 'Artist' (if 'Album Artist' is empty) for <b>all</b> tracks?")
                                                              : i18n("Set 'Album Artist' from 'Artist' (if 'Album Artist' is empty)?"),
                                                  i18n("Album Artist from Artist"), StdGuiItem::apply(), StdGuiItem::cancel())) {
        return;
    }

    if (isAll) {
        updating=true;
        for (int i=0; i<edited.count(); ++i) {
            Song s=edited.at(i);
            if (s.setAlbumArtist()) {
                edited.replace(i, s);
                updateEditedStatus(i);
                if (i==currentSongIndex) {
                    setSong(s);
                }
            }
        }
        updating=false;
        setLabelStates();
        enableOkButton();
    } else {
        Song s=edited.at(currentSongIndex);
        if (s.setAlbumArtist()) {
            edited.replace(currentSongIndex, s);
            updateEditedStatus(currentSongIndex);
            setSong(s);
        }
    }
}

void TagEditor::capitalise()
{
    bool isAll=0==currentSongIndex && original.count()>1;

    if (MessageBox::No==MessageBox::questionYesNo(this, isAll ? i18n("Capitalize the first letter of 'Title', 'Artist', 'Album artist', and "
                                                                     "'Album' of <b>all</b> tracks?")
                                                              : i18n("Capitalize the first letter of 'Title', 'Artist', 'Album artist', and "
                                                                     "'Album'"),
                                                  i18n("Capitalize"), GuiItem(i18n("Capitalize")), StdGuiItem::cancel())) {
        return;
    }

    if (isAll) {
        for (int i=0; i<edited.count(); ++i) {
            Song s=edited.at(i);
            if (s.capitalise()) {
                edited.replace(i, s);
                updateEditedStatus(i);
                if (i==currentSongIndex) {
                    setSong(s);
                }
            }
        }
    } else {
        Song s=edited.at(currentSongIndex);
        if (s.capitalise()) {
            edited.replace(currentSongIndex, s);
            updateEditedStatus(currentSongIndex);
            setSong(s);
        }
    }
}

void TagEditor::adjustTrackNumbers()
{
    bool isAll=0==currentSongIndex && original.count()>1;
    bool ok=false;
    int adj=InputDialog::getInteger(i18n("Adjust Track Numbers"), isAll ? i18n("Adjust the value of each track number by:")
                                                                        : i18n("Adjust track number by:"),
                                    0, -500, 500, 1, 10, &ok, this);

    if (!ok || 0==adj) {
        return;
    }

    if (isAll) {
        for (int i=0; i<edited.count(); ++i) {
            Song s=edited.at(i);
            s.track+=adj;
            edited.replace(i, s);
            updateEditedStatus(i);
            if (i==currentSongIndex) {
                setSong(s);
            }
        }
    } else {
        Song s=edited.at(currentSongIndex);
        s.track+=adj;
        edited.replace(currentSongIndex, s);
        updateEditedStatus(currentSongIndex);
        setSong(s);
    }
}

void TagEditor::checkChanged()
{
    if (updating) {
        return;
    }

    bool allWasEdited=editedIndexes.contains(0);

    updateEdited();

    bool allEdited=editedIndexes.contains(0);
    bool isAll=0==currentSongIndex && original.count()>1;

    if (isAll && (allEdited || allWasEdited)) {
        int save=currentSongIndex;
        for (int i=1; i<edited.count(); ++i) {
            currentSongIndex=i;
            updateEdited(true);
        }
        currentSongIndex=save;
    }
    enableOkButton();
    setLabelStates();
}

void TagEditor::updateTrackName(int index, bool edited)
{
    bool isAll=0==index && original.count()>1;

    if (edited) {
        if (isAll) {
            trackName->setItemText(index, i18n("All tracks [modified]"));
        } else {
            trackName->setItemText(index, i18n("%1 [modified]", original.at(index).file));
        }
    } else {
        if (isAll) {
            trackName->setItemText(index, i18n("All tracks"));
        } else {
            trackName->setItemText(index, original.at(index).file);
        }
    }
}

void TagEditor::updateEditedStatus(int index)
{
    bool isAll=0==index && original.count()>1;
    Song s=edited.at(index);
    if (equalTags(s, original.at(index), isAll)) {
        if (editedIndexes.contains(index)) {
            editedIndexes.remove(index);
            updateTrackName(index, false);
            edited.replace(index, s);
        }
    } else {
        if (!editedIndexes.contains(index)) {
            editedIndexes.insert(index);
            updateTrackName(index, true);
        }
        edited.replace(index, s);
    }
}

void TagEditor::updateEdited(bool isFromAll)
{
    Song s=edited.at(currentSongIndex);
    bool isAll=0==currentSongIndex && original.count()>1;
    fillSong(s, isFromAll || isAll, /*isFromAll*/false);

    if (!isAll && isFromAll && original.count()>1) {
        Song all=original.at(0);
        Song o=original.at(currentSongIndex);
        if (all.artist.isEmpty() && s.artist.isEmpty() && !o.artist.isEmpty()) {
            s.artist=o.artist;
        }
        if (all.albumartist.isEmpty() && s.albumartist.isEmpty() && !o.albumartist.isEmpty()) {
            s.albumartist=o.albumartist;
        }
        if (all.album.isEmpty() && s.album.isEmpty() && !o.album.isEmpty()) {
            s.album=o.album;
        }
        if (all.genre.isEmpty() && s.genre.isEmpty() && !o.genre.isEmpty()) {
            s.genre=o.genre;
        }
    }

    if (equalTags(s, original.at(currentSongIndex), isFromAll || isAll)) {
        if (editedIndexes.contains(currentSongIndex)) {
            editedIndexes.remove(currentSongIndex);
            updateTrackName(currentSongIndex, false);
            edited.replace(currentSongIndex, s);
        }
    } else {
        if (!editedIndexes.contains(currentSongIndex)) {
            editedIndexes.insert(currentSongIndex);
            updateTrackName(currentSongIndex, true);
        }
        edited.replace(currentSongIndex, s);
    }
}

void TagEditor::setSong(const Song &s)
{
    title->setText(s.title);
    artist->setText(s.artist);
    albumArtist->setText(s.albumartist);
    album->setText(s.album);
    track->setValue(s.track);
    disc->setValue(s.disc);
    genre->setText(s.genre);
    year->setValue(s.year);
}

void TagEditor::setIndex(int idx)
{
    if (currentSongIndex==idx || idx>(original.count()-1)) {
        return;
    }
    updating=true;

    bool haveMultiple=original.count()>1;

    if (haveMultiple && currentSongIndex>=0) {
        updateEdited();
    }
    Song s=edited.at(!haveMultiple || idx==0 ? 0 : idx);
    setSong(s);
    currentSongIndex=idx;

    bool isMultiple=haveMultiple && 0==idx;
    title->setEnabled(!isMultiple);
    titleLabel->setEnabled(!isMultiple);
    track->setEnabled(!isMultiple);
    trackLabel->setEnabled(!isMultiple);
    if (isMultiple) {
        title->setText(QString());
        track->setValue(0);
    }

    if (original.count()>1) {
        enableButton(User1, !isMultiple && idx<(original.count()-1)); // Next
        enableButton(User2, !isMultiple && idx>1); // Prev
    }
    setPlaceholderTexts();
    enableOkButton();
    trackName->setCurrentIndex(idx);
    setLabelStates();
    updating=false;
}

void TagEditor::applyUpdates()
{
    bool skipFirst=original.count()>1;
    QStringList failed;
    DeviceOptions opts;
    QString udi;
    #ifdef ENABLE_DEVICES_SUPPORT
    Device * dev=0;
    if (!deviceUdi.isEmpty()) {
        dev=getDevice(deviceUdi, this);
        if (!dev) {
            return;
        }
        opts=dev->options();
        udi=dev->id();
    } else
    #endif
    opts.load(MPDConnectionDetails::configGroupName(MPDConnection::self()->getDetails().name), true);

    int toSave=editedIndexes.count();
    bool renameFiles=false;
    QList<Song> updatedSongs;

    saving=true;
    enableButton(Ok, false);
    enableButton(Cancel, false);
    enableButton(Reset, false);
    enableButton(User1, false);
    enableButton(User2, false);
    enableButton(User3, false);
    progress->setVisible(true);
    progress->setRange(1, toSave);

    int count=0;
    foreach (int idx, editedIndexes) {
        progress->setValue(progress->value()+1);

        if (0==count++%10) {
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        }

        if (skipFirst && 0==idx) {
            continue;
        }
        Song orig=original.at(idx);
        Song edit=edited.at(idx);
        if (equalTags(orig, edit, false)) {
            continue;
        }

        switch(Tags::update(baseDir+orig.file, orig, edit)) {
        case Tags::Update_Modified:
            #ifdef ENABLE_DEVICES_SUPPORT
            if (!deviceUdi.isEmpty()) {
                if (!dev->updateSong(orig, edit)) {
                    dev->removeSongFromList(orig);
                    dev->addSongToList(edit);
                }
            } else
            #endif
            {
                if (!MusicLibraryModel::self()->updateSong(orig, edit)) {
                    MusicLibraryModel::self()->removeSongFromList(orig);
                    MusicLibraryModel::self()->addSongToList(edit);
                }
            }
            updatedSongs.append(edit);
            if (!renameFiles && orig.file!=opts.createFilename(edit)) {
                renameFiles=true;
            }
            break;
        case Tags::Update_Failed:
            failed.append(orig.file);
            break;
        default:
            break;
        }
    }
    saving=false;

    if (failed.count()) {
        MessageBox::errorListEx(this, i18n("Failed to update the tags of the following tracks:"), failed);
    }

    if (updatedSongs.count()) {
        #ifdef ENABLE_DEVICES_SUPPORT
        if (!deviceUdi.isEmpty()) {
            dev->saveCache();
        } else
        #endif
        {
//             MusicLibraryModel::self()->removeCache();
            emit update();
        }

        if (renameFiles &&
            MessageBox::Yes==MessageBox::questionYesNo(this, i18n("Would you also like to rename your song files, so as to match your tags?"),
                                                       i18n("Rename Files"), GuiItem(i18n("Rename")), StdGuiItem::cancel())) {
            TrackOrganiser *dlg=new TrackOrganiser(parentWidget());
            dlg->show(updatedSongs, udi);
        }
    }
}

void TagEditor::slotButtonClicked(int button)
{
    switch (button) {
    case Ok: {
        applyUpdates();
        accept();
        break;
    }
    case Reset: // Reset
        if (0==currentSongIndex && original.count()>1) {
            for (int i=0; i<original.count(); ++i) {
                edited.replace(i, original.at(i));
                updateTrackName(i, false);
            }
            editedIndexes.clear();
            setSong(original.at(currentSongIndex));
        } else {
            setSong(original.at(currentSongIndex));
        }
        enableOkButton();
        break;
    case User1: // Next
        setIndex(currentSongIndex+1);
        break;
    case User2: // Prev
        setIndex(currentSongIndex-1);
        break;
    case Cancel:
        reject();
        // Need to call this - if not, when dialog is closed by window X control, it is not deleted!!!!
        Dialog::slotButtonClicked(button);
        break;
    default:
        break;
    }
}

#ifdef ENABLE_DEVICES_SUPPORT
Device * TagEditor::getDevice(const QString &udi, QWidget *p)
{
    Device *dev=DevicesModel::self()->device(udi);
    if (!dev) {
        MessageBox::error(p ? p : this, i18n("Device has been removed!"));
        reject();
        return 0;
    }
    if (!dev->isConnected()) {
        MessageBox::error(p ? p : this, i18n("Device is not connected."));
        reject();
        return 0;
    }
    if (!dev->isIdle()) {
        MessageBox::error(p ? p : this, i18n("Device is busy?"));
        reject();
        return 0;
    }
    return dev;
}
#endif

void TagEditor::closeEvent(QCloseEvent *event)
{
    if (saving) {
        event->ignore();
    } else {
        Dialog::closeEvent(event);
    }
}
