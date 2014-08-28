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

#include "tageditor.h"
#include "tags.h"
#include "widgets/tagspinbox.h"
#include "models/musiclibrarymodel.h"
#include "mpd/mpdconnection.h"
#include "gui/settings.h"
#include "support/messagebox.h"
#include "support/inputdialog.h"
#include "support/localize.h"
#include "trackorganiser.h"
#include "mpd/cuefile.h"
#include "support/utils.h"
#ifdef ENABLE_DEVICES_SUPPORT
#include "models/devicesmodel.h"
#include "devices/device.h"
#endif
#include <QMenu>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QEventLoop>
#include <QDir>
#include <QTimer>

#define REMOVE(w) \
    w->setVisible(false); \
    w->deleteLater(); \
    w=0;

//
// NOTE: Cantata does NOT read, and store, comments from MPD. For comment support, Cantata will read these from the
// files when the tag editor is opened.
//
static bool equalTags(const Song &a, const Song &b, bool compareCommon, bool composerSupport, bool commentSupport)
{
    return (compareCommon || a.track==b.track) && a.year==b.year && a.disc==b.disc &&
           a.artist==b.artist && a.genre==b.genre && a.album==b.album &&
           a.albumartist==b.albumartist && (!composerSupport || a.composer()==b.composer()) &&
           (!commentSupport || a.comment()==b.comment()) && (compareCommon || a.title==b.title);
}

static bool setString(QString &str, const QString &v, bool skipEmpty) {
    if (!skipEmpty || !v.isEmpty()) {
        str=v;
        return true;
    }
    return false;
}

static QString trim(QString str) {
    str=str.trimmed();
    str=str.simplified();
    str=str.replace(QLatin1String(" ;"), QLatin1String(";"));
    str=str.replace(QLatin1String("; "), QLatin1String(";"));
    return str;
}

static int iCount=0;

int TagEditor::instanceCount()
{
    return iCount;
}

static inline bool nullRating(const quint8 &rating)
{
    return rating>Song::Rating_Max;
}

static inline bool nullRating(const Song &s)
{
    return nullRating(s.rating);
}

TagEditor::TagEditor(QWidget *parent, const QList<Song> &songs,
                     const QSet<QString> &existingArtists, const QSet<QString> &existingAlbumArtists, const QSet<QString> &existingComposers,
                     const QSet<QString> &existingAlbums, const QSet<QString> &existingGenres, const QString &udi)
    : SongDialog(parent)
    #ifdef ENABLE_DEVICES_SUPPORT
    , deviceUdi(udi)
    #endif
    , currentSongIndex(-1)
    , updating(false)
    , haveArtists(false)
    , haveAlbumArtists(false)
    , haveComposers(false)
    , haveComments(false)
    , haveAlbums(false)
    , haveGenres(false)
    , haveDiscs(false)
    , haveYears(false)
    , haveRatings(false)
    , saving(false)
    , composerSupport(false)
    , commentSupport(false)
{
    iCount++;
    bool isMopidy=false;
    #ifdef ENABLE_DEVICES_SUPPORT
    if (deviceUdi.isEmpty()) {
        baseDir=MPDConnection::self()->getDetails().dir;
        composerSupport=MPDConnection::self()->composerTagSupported();
        commentSupport=MPDConnection::self()->commentTagSupported();
        isMopidy=MPDConnection::self()->isMopdidy();
    } else {
        Device *dev=getDevice(udi, parentWidget());

        if (!dev) {
            deleteLater();
            return;
        }

        baseDir=dev->path();
    }
    #else
    baseDir=MPDConnection::self()->getDetails().dir;
    composerSupport=MPDConnection::self()->composerTagSupported();
    commentSupport=MPDConnection::self()->commentTagSupported();
    isMopidy=MPDConnection::self()->isMopdidy();
    #endif

    foreach (const Song &s, songs) {
        if (CueFile::isCue(s.file)) {
            continue;
        }
        Song song(s);
        s.rating=Song::Rating_Null;
        if (s.guessed) {
            song.revertGuessedTags();
        }
        original.append(song);
    }

    if (original.isEmpty()) {
        deleteLater();
        return;
    }
    qSort(original);

    if (!songsOk(original, baseDir, udi.isEmpty())) {
        return;
    }

    QWidget *mainWidet = new QWidget(this);
    setupUi(mainWidet);
    if (isMopidy) {
        connect(mopidyNote, SIGNAL(leftClickedUrl()), SLOT(showMopidyMessage()));
        REMOVE(ratingWidget);
        REMOVE(ratingLabel);
        REMOVE(ratingVarious);
    } else {
        REMOVE(mopidyNote);
        connect(this, SIGNAL(getRating(QString)), MPDConnection::self(), SLOT(getRating(QString)));
        connect(this, SIGNAL(setRating(QString,quint8)), MPDConnection::self(), SLOT(setRating(QString,quint8)));
        connect(MPDConnection::self(), SIGNAL(rating(QString,quint8)), this, SLOT(rating(QString,quint8)));
        ratingWidget->setShowZeroForNull(true);
        QColor col=palette().color(QPalette::WindowText);
        ratingVarious->setStyleSheet(QString("QLabel{color:rgba(%1,%2,%3,128);}").arg(col.red()).arg(col.green()).arg(col.blue()));
    }
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
    toolsMenu->addAction(i18n("Read Ratings from File"), this, SLOT(readRatings()));
    toolsMenu->addAction(i18n("Write Ratings to File"), this, SLOT(writeRatings()));
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

    if (composerSupport) {
        strings=existingComposers.toList();
        strings.sort();
        composer->clear();
        composer->insertItems(0, strings);
    } else {
        REMOVE(composer)
        REMOVE(composerLabel)
    }

    if (commentSupport) {
        comment->clear();
        composer->insertItems(0, strings);
    } else {
        REMOVE(comment)
        REMOVE(commentLabel)
    }

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
        QSet<QString> songComposers;
        QSet<QString> songComments;
        QSet<int> songYears;
        QSet<int> songDiscs;

        foreach (const Song &s, original) {
            songArtists.insert(s.artist);
            songAlbumArtists.insert(s.albumartist);
            songAlbums.insert(s.album);
            songGenres.insert(s.genre);
            if (composerSupport) {
                songComposers.insert(s.composer());
            }
            if (commentSupport) {
                songComments.insert(s.comment());
            }
            songYears.insert(s.year);
            songDiscs.insert(s.disc);
            if (songArtists.count()>1 && songAlbumArtists.count()>1 && songAlbums.count()>1 &&
                songGenres.count()>1 && songYears.count()>1 && songDiscs.count()>1 &&
                (!composerSupport || songComposers.count()>1) && (!commentSupport || songComments.count()>1)) {
                break;
            }
        }
        Song all;
        all.file.clear();
        all.title.clear();
        all.track=0;
        all.artist=1==songArtists.count() ? *(songArtists.begin()) : QString();
        if (1==songComposers.count()) {
            all.setComposer(*(songComposers.begin()));
        }
        all.setComment(1==songComments.count() ? *(songComments.begin()) : QString());
        all.albumartist=1==songAlbumArtists.count() ? *(songAlbumArtists.begin()) : QString();
        all.album=1==songAlbums.count() ? *(songAlbums.begin()) : QString();
        all.genre=1==songGenres.count() ? *(songGenres.begin()) : QString();
        all.year=1==songYears.count() ? *(songYears.begin()) : 0;
        all.disc=1==songDiscs.count() ? *(songDiscs.begin()) : 0;
        all.rating=Song::Rating_Null;
        original.prepend(all);
        artist->setFocus();
        haveArtists=songArtists.count()>1;
        haveAlbumArtists=songAlbumArtists.count()>1;
        haveAlbums=songAlbums.count()>1;
        haveGenres=songGenres.count()>1;
        haveComposers=songComposers.count()>1;
        haveDiscs=songDiscs.count()>1;
        haveYears=songYears.count()>1;
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
            trackName->insertItem(trackName->count(), s.filePath());
            if (!isMopidy) {
                emit getRating(s.file);
            }
        }
    }
    connect(title, SIGNAL(textChanged(const QString &)), SLOT(checkChanged()));
    connect(artist, SIGNAL(activated(int)), SLOT(checkChanged()));
    connect(artist, SIGNAL(editTextChanged(const QString &)), SLOT(checkChanged()));
    connect(albumArtist, SIGNAL(activated(int)), SLOT(checkChanged()));
    connect(albumArtist, SIGNAL(editTextChanged(const QString &)), SLOT(checkChanged()));
    if (composerSupport) {
        connect(composer, SIGNAL(activated(int)), SLOT(checkChanged()));
        connect(composer, SIGNAL(editTextChanged(const QString &)), SLOT(checkChanged()));
    }
    if (commentSupport) {
        connect(comment, SIGNAL(textChanged(const QString &)), SLOT(checkChanged()));
    }
    connect(album, SIGNAL(activated(int)), SLOT(checkChanged()));
    connect(album, SIGNAL(editTextChanged(const QString &)), SLOT(checkChanged()));
    connect(genre, SIGNAL(activated(int)), SLOT(checkChanged()));
    connect(track, SIGNAL(valueChanged(int)), SLOT(checkChanged()));
    connect(disc, SIGNAL(valueChanged(int)), SLOT(checkChanged()));
    connect(genre, SIGNAL(editTextChanged(const QString &)), SLOT(checkChanged()));
    connect(year, SIGNAL(valueChanged(int)), SLOT(checkChanged()));
    connect(trackName, SIGNAL(activated(int)), SLOT(setIndex(int)));
    connect(this, SIGNAL(update()), MPDConnection::self(), SLOT(update()));
    if (ratingWidget) {
        connect(ratingWidget, SIGNAL(valueChanged(int)), SLOT(checkRating()));
    }
    adjustSize();
    int w=Utils::scaleForDpi(600);
    if (width()<w) {
        resize(w, height());
    }
    if (commentSupport) {
        QTimer::singleShot(0, this, SLOT(readComments()));
    }
    trackName->lineEdit()->setReadOnly(true);
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
    if (composerSupport) {
        QString str;
        if (setString(str, composer->text().trimmed(), skipEmpty && (!haveAll || all.composer().isEmpty()))) {
            s.setComposer(str);
        }
    }
    if (commentSupport) {
        QString str;
        if (setString(str, comment->text().trimmed(), skipEmpty && (!haveAll || all.comment().isEmpty()))) {
            s.setComment(str);
        }
    }
    if (!isAll) {
        s.track=track->value();
    }
    if (!isAll || 0!=disc->value()) {
        s.disc=disc->value();
    }
    setString(s.genre, trim(genre->text()), skipEmpty && (!haveAll || all.genre.isEmpty()));
    if (!isAll || 0!=year->value()) {
        s.year=year->value();
    }
    if (ratingWidget) {
        s.rating=ratingWidget->value();
    }
}

void TagEditor::setVariousHint()
{
    if (0==currentSongIndex && original.count()>1) {
        Song all=original.at(0);
        artist->setPlaceholderText(all.artist.isEmpty() && haveArtists ? TagSpinBox::variousStr() : QString());
        album->setPlaceholderText(all.album.isEmpty() && haveAlbums ? TagSpinBox::variousStr() : QString());
        albumArtist->setPlaceholderText(all.albumartist.isEmpty() && haveAlbumArtists ? TagSpinBox::variousStr() : QString());
        if (composerSupport) {
            composer->setPlaceholderText(all.composer().isEmpty() && haveComposers ? TagSpinBox::variousStr() : QString());
        }
        if (commentSupport) {
            comment->setPlaceholderText(all.comment().isEmpty() && haveComments ? TagSpinBox::variousStr() : QString());
        }
        genre->setPlaceholderText(all.genre.isEmpty() && haveGenres ? TagSpinBox::variousStr() : QString());
        disc->setVarious(0==all.disc && haveDiscs);
        year->setVarious(0==all.year && haveYears);
        if (ratingWidget) {
            ratingVarious->setVisible(nullRating(all) && haveRatings);
        }
    } else if (ratingVarious) {
        ratingVarious->setVisible(false);
    }
}

void TagEditor::enableOkButton()
{
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
    if (composerSupport) {
        composerLabel->setOn(o.composer()!=e.composer());
    }
    if (commentSupport) {
        commentLabel->setOn(o.comment()!=e.comment());
    }
    albumArtistLabel->setOn(o.albumartist!=e.albumartist);
    albumLabel->setOn(o.album!=e.album);
    trackLabel->setOn(!isAll && o.track!=e.track);
    discLabel->setOn(o.disc!=e.disc);
    genreLabel->setOn(o.genre!=e.genre);
    yearLabel->setOn(o.year!=e.year);
    ratingLabel->setOn(o.rating!=e.rating);
}

void TagEditor::readComments()
{
    progress->setVisible(true);
    progress->setRange(0, original.count());
    bool haveMultiple=original.count()>1;
    bool updated=false;

    for (int i=0; i<original.count(); ++i) {
        progress->setValue(i+1);
        if (i && 0==i%10) {
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        }

        if (0==i && haveMultiple) {
            continue;
        }
        Song song=original.at(i);
        QString comment=Tags::readComment(baseDir+song.file);
        if (!comment.isEmpty()) {
            song.setComment(comment);
            original.replace(i, song);
            haveComments=true;
            updated=true;
        }
    }
    if (updated) {
        edited=original;
    }
    progress->setVisible(false);
    if (haveMultiple) {
        setVariousHint();
    } else {
        setSong(original.at(0));
    }
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

    if (MessageBox::No==MessageBox::questionYesNo(this, isAll ? i18n("Capitalize the first letter of text fields (e.g. 'Title', 'Artist', etc) "
                                                                     "of <b>all</b> tracks?")
                                                              : i18n("Capitalize the first letter of text fields (e.g. 'Title', 'Artist', etc)?"),
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
        for (int i=1; i<edited.count(); ++i) {
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

void TagEditor::readRatings()
{
    bool isAll=0==currentSongIndex && original.count()>1;

    if (MessageBox::No==MessageBox::questionYesNo(this, isAll ? i18n("Read ratings for all tracks from the music files?")
                                                              : i18n("Read rating from music file?"),
                                                  i18n("Ratings"),
                                                  isAll ? GuiItem(i18n("Read Ratings")) : GuiItem(i18n("Read Rating")),
                                                  StdGuiItem::cancel())) {
        return;
    }

    if (isAll) {
        progress->setVisible(true);
        progress->setRange(0, original.count());
        QStringList updated;
        for (int i=1; i<original.count(); ++i) {
            progress->setValue(i+1);
            if (i && 0==i%10) {
                QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            }
            Song s=edited.at(i);
            int r=Tags::readRating(baseDir+s.file);
            if (r>=0 && r<=Song::Rating_Max && s.rating!=r) {
                s.rating=r;
                edited.replace(i, s);
                updated.append(s.file);
                if (i==currentSongIndex) {
                    setSong(s);
                }
            }
        }
        progress->setVisible(false);
        if (!updated.isEmpty()) {
            MessageBox::informationList(this, i18n("Read, and updated, ratings from the following tracks:"), updated);
        }
    } else {
        Song s=edited.at(currentSongIndex);
        int r=Tags::readRating(baseDir+s.file);
        if (r>=0 && r<=Song::Rating_Max && s.rating!=r) {
            s.rating=r;
            edited.replace(currentSongIndex, s);
            setSong(s);
        }
    }
}

void TagEditor::writeRatings()
{
    bool isAll=0==currentSongIndex && original.count()>1;

    if (isAll) {
        for (int i=1; i<original.count(); ++i) {
            if (nullRating(original.at(i))) {
                MessageBox::error(this, i18n("Not all Song ratings have been read from MPD!")+QLatin1String("\n\n")+
                                  i18n("Song ratings are not stored in the song files, but within MPD's 'sticker' database. "
                                       "In order to save these into the actual file, Cantata must first read them from MPD."));
                return;
            }
        }
    } else {
        if (nullRating(original.at(currentSongIndex))) {
            MessageBox::error(this, i18n("Song rating has not been read from MPD!")+QLatin1String("\n\n")+
                              i18n("Song ratings are not stored in the song files, but within MPD's 'sticker' database. "
                                   "In order to save these into the actual file, Cantata must first read them from MPD."));
            return;
        }
    }

    if (MessageBox::No==MessageBox::questionYesNo(this, isAll ? i18n("Write ratings for all tracks to the music files?")
                                                              : i18n("Write rating to music file?"),
                                                  i18n("Ratings"),
                                                  isAll ? GuiItem(i18n("Write Ratings")) : GuiItem(i18n("Write Rating")),
                                                  StdGuiItem::cancel())) {
        return;
    }

    if (isAll) {
        progress->setVisible(true);
        progress->setRange(0, edited.count());
        QStringList failed;
        for (int i=1; i<edited.count(); ++i) {
            progress->setValue(i+1);
            if (i && 0==i%10) {
                QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            }
            Song s=edited.at(i);
            if (s.rating<=Song::Rating_Max) {
                Tags::Update status=Tags::updateRating(baseDir+s.file, s.rating);
                if (Tags::Update_Failed==status || Tags::Update_BadFile==status){
                    failed.append(s.file);
                }
            }
        }
        progress->setVisible(false);
        if (!failed.isEmpty()) {
            MessageBox::errorList(this, i18n("Failed to write ratings of the following tracks:"), failed);
        }
    } else {
        Song s=edited.at(currentSongIndex);
        if (s.rating<=Song::Rating_Max) {
            Tags::Update status=Tags::updateRating(baseDir+s.file, s.rating);
            if (Tags::Update_Failed==status || Tags::Update_BadFile==status){
                MessageBox::error(this, i18n("Failed to write rating to music file!"));
            }
        }
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
            trackName->setItemText(index, i18n("%1 [modified]", original.at(index).filePath()));
        }
    } else {
        if (isAll) {
            trackName->setItemText(index, i18n("All tracks"));
        } else {
            trackName->setItemText(index, original.at(index).filePath());
        }
    }
}

void TagEditor::updateEditedStatus(int index)
{
    bool isAll=0==index && original.count()>1;
    Song s=edited.at(index);
    if (equalTags(s, original.at(index), isAll, composerSupport, commentSupport) && s.rating==original.at(index).rating) {
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
        if (composerSupport && all.composer().isEmpty() && s.composer().isEmpty() && !o.composer().isEmpty()) {
            s.setComposer(o.composer());
        }
        if (commentSupport && all.comment().isEmpty() && s.comment().isEmpty() && !o.comment().isEmpty()) {
            s.setComment(o.comment());
        }
        if (all.album.isEmpty() && s.album.isEmpty() && !o.album.isEmpty()) {
            s.album=o.album;
        }
        if (all.genre.isEmpty() && s.genre.isEmpty() && !o.genre.isEmpty()) {
            s.genre=o.genre;
        }
    }

    if (equalTags(s, original.at(currentSongIndex), isFromAll || isAll, composerSupport, commentSupport) && s.rating==original.at(currentSongIndex).rating) {
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
    blockSignals(true);
    title->setText(s.title);
    artist->setText(s.artist);
    albumArtist->setText(s.albumartist);
    if (composerSupport) {
        composer->setText(s.composer());
    }
    if (commentSupport) {
        comment->setText(s.comment());
    }
    album->setText(s.album);
    track->setValue(s.track);
    disc->setValue(s.disc);
    genre->setText(s.genre);
    year->setValue(s.year);
    if (ratingWidget) {
        ratingWidget->setValue(s.rating);
    }
    blockSignals(false);
    checkChanged();
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
    setVariousHint();
    enableOkButton();
    trackName->setCurrentIndex(idx);
    setLabelStates();
    updating=false;
}

void TagEditor::showMopidyMessage()
{
    MessageBox::information(this, i18n("Cantata has detected that you are connected to a Mopidy server.\n\n"
                                       "Currently it is not possible for Cantata to force Mopidy to refresh its local "
                                       "music listing. Therefore, you will need to stop Cantata, manually refresh "
                                       "Mopidy's database, and restart Cantata for any changes to be active."),
                            QLatin1String("Mopidy"));
}

void TagEditor::rating(const QString &f, quint8 r)
{
    if (!ratingWidget) {
        return;
    }
    for (int i=original.count()>1 ? 1 : 0; i<original.count(); ++i) {
        Song s=original.at(i);
        if (nullRating(s) && s.rating!=r && s.file==f) {
            s.rating=r;
            original.replace(i, s);
            s=edited.at(i);
            if (nullRating(s)) {
                s.rating=r;
                edited.replace(i, s);
            }
            if (i==currentSongIndex && 0==ratingWidget->value()) {
                ratingWidget->setValue(r);
            }
        }
    }

    if (original.count()>1 && !haveRatings) {
        quint8 rating=Song::Rating_Null;
        bool first=true;
        for (int i=1; i<original.count() && !haveRatings; ++i) {
            quint8 r=original.at(i).rating;
            if (nullRating(r)) {
                continue;
            }
            if (first) {
                rating=r;
                first=false;
            } else if (r!=rating) {
                haveRatings=true;
                rating=Song::Rating_Null;
            }
        }
        Song s=original.at(0);
        if (s.rating!=rating) {
            s.rating=rating;
            original.replace(0, s);
            s=edited.at(0);
            if (nullRating(s) && s.rating!=rating) {
                s.rating=rating;
                edited.replace(0, s);
            }
            if (0==currentSongIndex) {
                ratingWidget->setValue(rating);
            }
        }
        setVariousHint();
    }
}

void TagEditor::checkRating()
{
    checkChanged();
    if (original.count()>1 && 0==currentSongIndex) {
        quint8 rating=Song::Rating_Null;
        bool first=true;
        haveRatings=false;
        for (int i=1; i<edited.count() && !haveRatings; ++i) {
            quint8 r=edited.at(i).rating;
            if (nullRating(r)) {
                continue;
            }
            if (first) {
                rating=r;
                first=false;
            } else if (r!=rating) {
                haveRatings=true;
                rating=Song::Rating_Null;
            }
        }
        Song s=edited.at(0);
        if (s.rating!=rating) {
            s.rating=rating;
            edited.replace(0, s);
            if (0==currentSongIndex) {
                ratingWidget->setValue(rating);
            }
        }
        setVariousHint();
    }
}

bool TagEditor::applyUpdates()
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
            return true;
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
    bool someTimedout=false;
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

        if (ratingWidget && orig.rating!=edit.rating) {
            emit setRating(orig.file, edit.rating);
        }

        if (equalTags(orig, edit, false, composerSupport, commentSupport)) {
            continue;
        }

        QString file=orig.filePath();
        switch(Tags::update(baseDir+file, orig, edit, -1, commentSupport)) {
        case Tags::Update_Modified:
            edit.setComment(QString());
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
            if (!renameFiles && file!=opts.createFilename(edit)) {
                renameFiles=true;
            }
            break;
        case Tags::Update_Failed:
            failed.append(file);
            break;
        case Tags::Update_BadFile:
            failed.append(i18nc("filename (Corrupt tags?)", "%1 (Corrupt tags?)", file));
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
        // If we call tag-editor, no need to do MPD update - as this will be done from that dialog...
        if (renameFiles &&
            MessageBox::Yes==MessageBox::questionYesNo(this, i18n("Would you also like to rename your song files, so as to match your tags?"),
                                                       i18n("Rename Files"), GuiItem(i18n("Rename")), StdGuiItem::cancel())) {
            TrackOrganiser *dlg=new TrackOrganiser(parentWidget());
            dlg->show(updatedSongs, udi, true);
        } else {
            #ifdef ENABLE_DEVICES_SUPPORT
            if (!deviceUdi.isEmpty()) {
                dev->saveCache();
            } else
            #endif
            {
            //             MusicLibraryModel::self()->removeCache();
                emit update();
            }
        }
    }

    return !someTimedout;
}

void TagEditor::slotButtonClicked(int button)
{
    switch (button) {
    case Ok: {
        if (applyUpdates()) {
            accept();
        }
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
            checkRating();
        } else {
            setSong(original.at(currentSongIndex));
        }
        setLabelStates();
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
