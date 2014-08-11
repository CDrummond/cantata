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

#include "songview.h"
#include "lyricsdialog.h"
#include "ultimatelyricsprovider.h"
#include "ultimatelyrics.h"
#include "contextengine.h"
#include "gui/settings.h"
#include "gui/covers.h"
#include "support/squeezedtextlabel.h"
#include "support/utils.h"
#include "support/messagebox.h"
#include "support/localize.h"
#ifdef TAGLIB_FOUND
#include "tags/tags.h"
#endif
#include "widgets/icons.h"
#include "support/utils.h"
#include "support/action.h"
#include "support/actioncollection.h"
#include "network/networkaccessmanager.h"
#include "widgets/textbrowser.h"
#include "gui/stdactions.h"
#include "mpd/mpdstatus.h"
#include "qtiocompressor/qtiocompressor.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QTextStream>
#include <QTimer>
#include <QScrollBar>
#include <QDesktopServices>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif

const QLatin1String SongView::constLyricsDir("lyrics/");
const QLatin1String SongView::constExtension(".lyrics");
const QLatin1String SongView::constCacheDir("tracks/");
const QLatin1String SongView::constInfoExt(".html.gz");

static QString infoCacheFileName(const Song &song, const QString &lang, bool createDir)
{
    QString artist=song.artist;
    QString title=song.title;
    title.replace("/", "_");
    artist.replace("/", "_");
    QString dir=Utils::cacheDir(SongView::constCacheDir+Covers::encodeName(artist)+Utils::constDirSep, createDir);

    if (dir.isEmpty()) {
        return QString();
    }
    return dir+Covers::encodeName(title)+"."+lang+SongView::constInfoExt;
}

static QString lyricsCacheFileName(const Song &song, bool createDir=false)
{
    QString artist=song.artist;
    QString title=song.title;
    title.replace("/", "_");
    artist.replace("/", "_");
    QString dir=Utils::cacheDir(SongView::constLyricsDir+Covers::encodeName(artist)+Utils::constDirSep, createDir);

    if (dir.isEmpty()) {
        return QString();
    }
    return dir+Covers::encodeName(title)+SongView::constExtension;
}

static inline QString mpdLyricsFilePath(const QString &songFile)
{
    return Utils::changeExtension(MPDConnection::self()->getDetails().dir+songFile, SongView::constExtension);
}

static inline QString mpdLyricsFilePath(const Song &song)
{
    return mpdLyricsFilePath(song.filePath());
}

static inline QString fixNewLines(const QString &o)
{
    return QString(o).replace(QLatin1String("\n\n\n"), QLatin1String("\n\n")).replace("\n", "<br/>");
}

static QString actualFile(const Song &song)
{
    QString songFile=song.filePath();

    if (song.isCantataStream()) {
        #if QT_VERSION < 0x050000
        QUrl u(songFile);
        #else
        QUrl qu(songFile);
        QUrlQuery u(qu);
        #endif
        songFile=u.hasQueryItem("file") ? u.queryItemValue("file") : QString();
    }
    return songFile;
}

SongView::SongView(QWidget *p)
    : View(p, QStringList() << i18n("Lyrics") << i18n("Information") << i18n("Tags"))
    , scrollTimer(0)
    , songPos(0)
    , currentProvider(-1)
    , currentRequest(0)
    , mode(Mode_Display)
    , job(0)
    , currentProv(0)
    , lyricsNeedsUpdating(true)
    , infoNeedsUpdating(true)
    , tagsNeedsUpdating(true)
{
    scrollAction = ActionCollection::get()->createAction("scrolllyrics", i18n("Scroll Lyrics"), "go-down");
    refreshAction = ActionCollection::get()->createAction("refreshlyrics", i18n("Refresh Lyrics"), "view-refresh");
    editAction = ActionCollection::get()->createAction("editlyrics", i18n("Edit Lyrics"), Icons::self()->editIcon);
    delAction = ActionCollection::get()->createAction("dellyrics", i18n("Delete Lyrics File"), "edit-delete");

    scrollAction->setCheckable(true);
    scrollAction->setChecked(Settings::self()->contextAutoScroll());
    connect(scrollAction, SIGNAL(toggled(bool)), SLOT(toggleScroll()));
    connect(refreshAction, SIGNAL(triggered()), SLOT(update()));
    connect(editAction, SIGNAL(triggered()), SLOT(edit()));
    connect(delAction, SIGNAL(triggered()), SLOT(del()));
    connect(UltimateLyrics::self(), SIGNAL(lyricsReady(int, QString)), SLOT(lyricsReady(int, QString)));

    engine=ContextEngine::create(this);
    refreshInfoAction = ActionCollection::get()->createAction("refreshtrack", i18n("Refresh Track Information"), "view-refresh");
    cancelInfoJobAction=new Action(Icons::self()->cancelIcon, i18n("Cancel"), this);
    cancelInfoJobAction->setEnabled(false);
    connect(refreshInfoAction, SIGNAL(triggered()), SLOT(refreshInfo()));
    connect(cancelInfoJobAction, SIGNAL(triggered()), SLOT(abortInfoSearch()));
    connect(engine, SIGNAL(searchResult(QString,QString)), this, SLOT(infoSearchResponse(QString,QString)));
    foreach (TextBrowser *t, texts) {
        connect(t, SIGNAL(anchorClicked(QUrl)), SLOT(showMoreInfo(QUrl)));
    }

    text->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(text, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showContextMenu(QPoint)));
    texts.at(Page_Information)->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(texts.at(Page_Information), SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showInfoContextMenu(QPoint)));
    connect(this, SIGNAL(viewChanged()), this, SLOT(curentViewChanged()));
    setMode(Mode_Blank);
    setStandardHeader(i18n("Track"));
    clear();
    toggleScroll();
    setCurrentView(Settings::self()->contextTrackView());
}

SongView::~SongView()
{
    UltimateLyrics::self()->release();
}

void SongView::update()
{
    QString mpdName=mpdFileName();
    QString cacheName=cacheFileName();
    bool mpdExists=!mpdName.isEmpty() && QFile::exists(mpdName);
    bool cacheExists=!cacheName.isEmpty() && QFile::exists(cacheName);

    if (mpdExists || cacheExists) {
        switch (MessageBox::warningYesNoCancel(this, i18n("Reload lyrics?\n\nReload from disk, or delete disk copy and download?"), i18n("Reload"),
                                               GuiItem(i18n("Reload From Disk")), GuiItem(i18n("Download")))) {
        case MessageBox::Yes:
            break;
        case MessageBox::No:
            if (mpdExists) {
                QFile::remove(mpdName);
            }
            if (cacheExists) {
                QFile::remove(cacheName);
            }
            break;
        default:
            return;
        }
    }

    update(currentSong, true);
}

void SongView::saveConfig()
{
    Settings::self()->saveContextAutoScroll(scrollAction->isChecked());
    Settings::self()->saveContextTrackView(currentView());
}

void SongView::search()
{
    setMode(Mode_Display);
    Song song=currentSong;
    LyricsDialog dlg(currentSong, this);
    if (QDialog::Accepted==dlg.exec()) {
        if ((song.artist!=currentSong.artist || song.title!=currentSong.title) &&
                MessageBox::No==MessageBox::warningYesNo(this, i18n("Current playing song has changed, still perform search?"), i18n("Song Changed"),
                                                         GuiItem(i18n("Perform Search")), StdGuiItem::cancel())) {
            return;
        }
        QString mpdName=mpdFileName();
        if (!mpdName.isEmpty() && QFile::exists(mpdName)) {
            QFile::remove(mpdName);
        }
        QString cacheName=cacheFileName();
        if (!cacheName.isEmpty() && QFile::exists(cacheName)) {
            QFile::remove(cacheName);
        }
        update(dlg.song(), true);
    }
}

void SongView::edit()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(lyricsFile));
}

void SongView::del()
{
    if (MessageBox::No==MessageBox::warningYesNo(this, i18n("Delete lyrics file?"), i18n("Delete File"),
                                                 StdGuiItem::del(), StdGuiItem::cancel())) {
        return;
    }

    QString mpdName=mpdFileName();
    QString cacheName=cacheFileName();
    if (!cacheName.isEmpty() && QFile::exists(cacheName)) {
        QFile::remove(cacheName);
    }
    if (!mpdName.isEmpty() && QFile::exists(mpdName)) {
        QFile::remove(mpdName);
    }
}

void SongView::showContextMenu(const QPoint &pos)
{
   QMenu *menu = text->createStandardContextMenu();
   switch (mode) {
   case Mode_Blank:
       break;
   case Mode_Display:
       menu->addSeparator();
       menu->addAction(scrollAction);
       menu->addAction(refreshAction);
       menu->addAction(StdActions::self()->searchAction);
       menu->addSeparator();
       menu->addAction(editAction);
       menu->addAction(delAction);
       break;
   }

   if (cancelJobAction->isEnabled()) {
       menu->addSeparator();
       menu->addAction(cancelJobAction);
   }

   menu->exec(text->mapToGlobal(pos));
   delete menu;
}

void SongView::showInfoContextMenu(const QPoint &pos)
{
    QMenu *menu = texts.at(Page_Information)->createStandardContextMenu();
    menu->addSeparator();
    if (cancelInfoJobAction->isEnabled()) {
        menu->addAction(cancelInfoJobAction);
    } else {
        menu->addAction(refreshInfoAction);
    }
    menu->exec(texts.at(Page_Information)->mapToGlobal(pos));
    delete menu;
}

void SongView::toggleScroll()
{
    if (scrollAction->isChecked()) {
        scrollTimer=new QTimer(this);
        scrollTimer->setSingleShot(false);
        scrollTimer->setInterval(1000);
        connect(MPDStatus::self(), SIGNAL(updated()), this, SLOT(songPosition()));
        connect(scrollTimer, SIGNAL(timeout()), this, SLOT(scroll()));
        scroll();
    } else {
        disconnect(MPDStatus::self(), SIGNAL(updated()), this, SLOT(songPosition()));
        if (scrollTimer) {
            connect(scrollTimer, SIGNAL(timeout()), this, SLOT(scroll()));
            scrollTimer->stop();
        }
    }
}

void SongView::songPosition()
{
    if (scrollTimer) {
        songPos=MPDStatus::self()->timeElapsed();
        scroll();
    }
}

void SongView::scroll()
{
    if (Mode_Display==mode && scrollAction->isChecked() && scrollTimer) {
        QScrollBar *bar=text->verticalScrollBar();

        if (bar && bar->isSliderDown()) {
            scrollTimer->stop();
            return;
        }
        if (MPDStatus::self()->timeTotal()<=0) {
            scrollTimer->stop();
            return;
        }
        if (MPDState_Playing==MPDStatus::self()->state()) {
            if (!scrollTimer->isActive()) {
                scrollTimer->start();
            }
        } else {
            scrollTimer->stop();
        }

        if (MPDStatus::self()->guessedElapsed()>=MPDStatus::self()->timeTotal()) {
            if (scrollTimer) {
                scrollTimer->stop();
            }
            return;
        }

        if (bar->isVisible()) {
            int newSliderPosition =
                        MPDStatus::self()->guessedElapsed() * (bar->maximum() + bar->pageStep()) / MPDStatus::self()->timeTotal() -
                        bar->pageStep() / 2;
            bar->setSliderPosition(newSliderPosition);
        }
    }
}

void SongView::curentViewChanged()
{
    if (!isVisible()) {
        return;
    }
    switch (currentView()) {
    case Page_Lyrics:      loadLyrics(); break;
    case Page_Information: loadInfo();   break;
    case Page_Tags:        loadTags();   break;
    default:                             break;
    }
}

void SongView::loadLyrics()
{
    if (!lyricsNeedsUpdating) {
        return;
    }
    lyricsNeedsUpdating=false;

    if (!MPDConnection::self()->getDetails().dir.isEmpty() && !currentSong.file.isEmpty() && !currentSong.isNonMPD()) {
        QString songFile=actualFile(currentSong);
        QString mpdLyrics=mpdLyricsFilePath(songFile);

        if (MPDConnection::self()->getDetails().dir.startsWith(QLatin1String("http:/"))) {
            QUrl url(mpdLyrics);
            job=NetworkAccessManager::self()->get(url);
            job->setProperty("file", currentSong.file);
            connect(job, SIGNAL(finished()), this, SLOT(downloadFinished()));
            return;
        } else {
            #ifdef TAGLIB_FOUND
            QString tagLyrics=Tags::readLyrics(MPDConnection::self()->getDetails().dir+songFile);

            if (!tagLyrics.isEmpty()) {
                text->setText(fixNewLines(tagLyrics));
                setMode(Mode_Display);
//                    controls->setVisible(false);
                return;
            }
            #endif
            // Stop here if we found lyrics in the cache dir.
            if (setLyricsFromFile(mpdLyrics)) {
                lyricsFile=mpdLyrics;
                setMode(Mode_Display);
                return;
            }
        }
    }

    // Check for cached file...
    QString file=lyricsCacheFileName(currentSong);

    /*if (force && QFile::exists(file)) {
        // Delete the cached lyrics file when the user is force-fully re-fetching the lyrics.
        // Afterwards we'll simply do getLyrics() to get the new ones.
        QFile::remove(file);
    } else */if (setLyricsFromFile(file)) {
       // We just wanted a normal update without explicit re-fetching. We can return
       // here because we got cached lyrics and we don't want an explicit re-fetch.
       lyricsFile=file;
       setMode(Mode_Display);
       return;
    }

    getLyrics();
}

void SongView::loadInfo()
{
    if (!infoNeedsUpdating) {
        return;
    }
    infoNeedsUpdating=false;
    foreach (const QString &lang, engine->getLangs()) {
        QString prefix=engine->getPrefix(lang);
        QString cachedFile=infoCacheFileName(currentSong, prefix, false);
        if (QFile::exists(cachedFile)) {
            QFile f(cachedFile);
            QtIOCompressor compressor(&f);
            compressor.setStreamFormat(QtIOCompressor::GzipFormat);
            if (compressor.open(QIODevice::ReadOnly)) {
                QByteArray data=compressor.readAll();

                if (!data.isEmpty()) {
                    infoSearchResponse(QString::fromUtf8(data), QString());
                    Utils::touchFile(cachedFile);
                    return;
                }
            }
        }
    }
    searchForInfo();
}

static QString createRow(const QString &key, const QString &value)
{
    return value.isEmpty() ? QString() : QString("<tr><td>%1:&nbsp;</td><td>%2</td></tr>").arg(key).arg(value);
}

struct MapEntry {
    MapEntry(int v=0, const QString &s=QString()) : val(v), str(s) { }
    int val;
    QString str;
};

#ifdef TAGLIB_FOUND
static QString clean(QString v)
{
    if (v.length()>1) {
        v.replace("_", " ");
        v=v[0]+v.toLower().mid(1);
    }
    return v;
}
#endif

void SongView::loadTags()
{
    if (!tagsNeedsUpdating) {
        return;
    }
    tagsNeedsUpdating=false;

    QMultiMap<int, QString> tags;
    QMultiMap<int, QString> audioProperties;
    #ifdef TAGLIB_FOUND
    if (!currentSong.isStandardStream() && !MPDConnection::self()->getDetails().dir.startsWith(QLatin1String("http:/"))) {
        QString songFile=actualFile(currentSong);
        if (!songFile.isEmpty()) {
            static QMap<QString, MapEntry> tagMap;
            static QMap<QString, MapEntry> tagTimeMap;
            static const QString constTitle=QLatin1String("TITLE");
            static const QString constPerformer=QLatin1String("PERFORMER:");
            static const QString constAudio=QLatin1String("X-AUDIO:");

            if (tagMap.isEmpty()) {
                int pos=0;
                tagMap.insert(QLatin1String("ARTIST"), MapEntry(pos++, i18n("Artist")));
                tagMap.insert(QLatin1String("ALBUMARTIST"), MapEntry(pos++, i18n("Album artist")));
                tagMap.insert(QLatin1String("COMPOSER"), MapEntry(pos++, i18n("Composer")));
                pos++;// For performer...
                tagMap.insert(QLatin1String("LYRICIST"), MapEntry(pos++, i18n("Lyricist")));
                tagMap.insert(QLatin1String("CONDUCTOR"), MapEntry(pos++, i18n("Conductor")));
                tagMap.insert(QLatin1String("REMIXER"), MapEntry(pos++, i18n("Remixer")));
                tagMap.insert(QLatin1String("ALBUM"), MapEntry(pos++, i18n("Album")));
                tagMap.insert(QLatin1String("SUBTITLE"), MapEntry(pos++, i18n("Subtitle")));
                tagMap.insert(QLatin1String("TRACKNUMBER"), MapEntry(pos++, i18n("Track number")));
                tagMap.insert(QLatin1String("DISCNUMBER"), MapEntry(pos++, i18n("Disc number")));
                tagMap.insert(QLatin1String("GENRE"), MapEntry(pos++, i18n("Genre")));
                tagMap.insert(QLatin1String("DATE"), MapEntry(pos++, i18n("Date")));
                tagMap.insert(QLatin1String("ORIGINALDATE"), MapEntry(pos++, i18n("Original date")));
                tagMap.insert(QLatin1String("COMMENT"), MapEntry(pos++, i18n("Comment")));
                tagMap.insert(QLatin1String("COPYRIGHT"), MapEntry(pos++, i18n("Copyright")));
                tagMap.insert(QLatin1String("LABEL"), MapEntry(pos++, i18n("Label")));
                tagMap.insert(QLatin1String("CATALOGNUMBER"), MapEntry(pos++, i18n("Catalogue number")));
                tagMap.insert(QLatin1String("TITLESORT"), MapEntry(pos++, i18n("Title sort")));
                tagMap.insert(QLatin1String("ARTISTSORT"), MapEntry(pos++, i18n("Artist sort")));
                tagMap.insert(QLatin1String("ALBUMARTISTSORT"), MapEntry(pos++, i18n("Album artist sort")));
                tagMap.insert(QLatin1String("ALBUMSORT"), MapEntry(pos++, i18n("Album sort")));
                tagMap.insert(QLatin1String("ENCODEDBY"), MapEntry(pos++, i18n("Encoded by")));
                tagMap.insert(QLatin1String("ENCODING"), MapEntry(pos++, i18n("Encoder")));
                tagMap.insert(QLatin1String("MOOD"), MapEntry(pos++, i18n("Mood")));
                tagMap.insert(QLatin1String("MEDIA"), MapEntry(pos++, i18n("Media")));
                tagMap.insert(constAudio+QLatin1String("BITRATE"), MapEntry(pos++, i18n("Bitrate")));
                tagMap.insert(constAudio+QLatin1String("SAMPLERATE"), MapEntry(pos++, i18n("Sample rate")));
                tagMap.insert(constAudio+QLatin1String("CHANNELS"), MapEntry(pos++, i18n("Channels")));

                tagTimeMap.insert(QLatin1String("TAGGING TIME"), MapEntry(pos++, i18n("Tagging time")));
            }

            QMap<QString, QString> allTags=Tags::readAll(MPDConnection::self()->getDetails().dir+actualFile(currentSong));

            if (!allTags.isEmpty()) {
                QMap<QString, QString>::ConstIterator it=allTags.constBegin();
                QMap<QString, QString>::ConstIterator end=allTags.constEnd();

                for (; it!=end; ++it) {
                    if (it.key()==constTitle) {
                        continue;
                    }
                    if (tagMap.contains(it.key())) {
                        if (it.key().startsWith(constAudio)) {
                            audioProperties.insert(tagMap[it.key()].val, createRow(tagMap[it.key()].str, it.value()));
                        } else {
                            tags.insert(tagMap[it.key()].val, createRow(tagMap[it.key()].str, it.value()));
                        }
                    } else if (tagTimeMap.contains(it.key())) {
                        tags.insert(tagTimeMap[it.key()].val, createRow(tagTimeMap[it.key()].str, QString(it.value()).replace("T", " ")));
                    } else if (it.key().startsWith(constPerformer)) {
                        tags.insert(3, createRow(i18n("Performer (%1)", clean(it.key().mid(constPerformer.length()))), it.value()));
                    } else {
                        tags.insert(tagMap.count()+tagTimeMap.count(), createRow(clean(it.key()), it.value()));
                    }
                }
            }
        }
    }
    #endif

    int audioPos=1024;
    if (audioProperties.isEmpty()) {
        if (MPDStatus::self()->bitrate()>0) {
            audioProperties.insert(audioPos++, createRow(i18n("Bitrate"), i18n("%1 kb/s", MPDStatus::self()->bitrate())));
        }
        if (MPDStatus::self()->samplerate()>0) {
            audioProperties.insert(audioPos++, createRow(i18n("Sample rate"), i18n("%1 Hz", MPDStatus::self()->samplerate())));
        }
        if (MPDStatus::self()->channels()>0) {
            audioProperties.insert(audioPos++, createRow(i18n("Channels"), QString::number(MPDStatus::self()->channels())));
        }
    }
    if (MPDStatus::self()->bits()>0) {
        audioProperties.insert(audioPos++, createRow(i18n("Bits"), QString::number(MPDStatus::self()->bits())));
    }

    if (tags.isEmpty()) {
        int pos=0;
        tags.insert(pos++, createRow(i18n("Artist"), currentSong.artist));
        tags.insert(pos++, createRow(i18n("Album artist"), currentSong.albumartist));
        tags.insert(pos++, createRow(i18n("Composer"), currentSong.composer()));
        tags.insert(pos++, createRow(i18n("Performer"), currentSong.performer()));
        tags.insert(pos++, createRow(i18n("Album"), currentSong.album));
        tags.insert(pos++, createRow(i18n("Track number"), 0==currentSong.track ? QString() : QString::number(currentSong.track)));
        tags.insert(pos++, createRow(i18n("Disc number"), 0==currentSong.disc ? QString() : QString::number(currentSong.disc)));
        tags.insert(pos++, createRow(i18n("Genre"), currentSong.genres().join(", ")));
        tags.insert(pos++, createRow(i18n("Year"), 0==currentSong.track ? QString() : QString::number(currentSong.year)));
        tags.insert(pos++, createRow(i18n("Comment"), currentSong.comment()));
    }

    QString tagInfo;
    if (!tags.isEmpty()) {
        tagInfo=QLatin1String("<table>");
        QMultiMap<int, QString>::ConstIterator it=tags.constBegin();
        QMultiMap<int, QString>::ConstIterator end=tags.constEnd();
        for (; it!=end; ++it) {
            if (!it.value().isEmpty()) {
                tagInfo+=it.value();
            }
        }
    }

    if (!audioProperties.isEmpty()) {
        if (tagInfo.isEmpty()) {
            tagInfo=QLatin1String("<table>");
        } else {
            tagInfo+=QLatin1String("<tr/>");
        }
        QMultiMap<int, QString>::ConstIterator it=audioProperties.constBegin();
        QMultiMap<int, QString>::ConstIterator end=audioProperties.constEnd();
        for (; it!=end; ++it) {
            if (!it.value().isEmpty()) {
                tagInfo+=it.value();
            }
        }
    }
    if (tagInfo.isEmpty()) {
        tagInfo=QLatin1String("<table>");
    } else {
        tagInfo+=QLatin1String("<tr/>");
    }
    if (MPDConnection::self()->getDetails().dirReadable) {
        QString path=Utils::getDir(MPDConnection::self()->getDetails().dir+currentSong.filePath());
        tagInfo+=createRow(i18n("Filename"), QLatin1String("<a href=\"file://")+path+QLatin1String("\">")+
                                             currentSong.filePath()+QLatin1String("</a>"));
    } else {
        tagInfo+=createRow(i18n("Filename"), currentSong.filePath());
    }
    tagInfo+=QLatin1String("</table>");

    setHtml(tagInfo, Page_Tags);
}

void SongView::refreshInfo()
{
    if (currentSong.isEmpty()) {
        return;
    }
    foreach (const QString &lang, engine->getLangs()) {
        QFile::remove(infoCacheFileName(currentSong, engine->getPrefix(lang), false));
    }
    searchForInfo();
}

void SongView::searchForInfo()
{
    cancelInfoJobAction->setEnabled(true);
    engine->search(QStringList() << currentSong.artist << currentSong.title, ContextEngine::Track);
    showSpinner(false);
}

void SongView::infoSearchResponse(const QString &resp, const QString &lang)
{
    cancelInfoJobAction->setEnabled(false);
    hideSpinner();
    QString str;
    if (!resp.isEmpty()) {
        str=engine->translateLinks(resp);
        if (!lang.isEmpty()) {
            QFile f(infoCacheFileName(currentSong, lang, true));
            QtIOCompressor compressor(&f);
            compressor.setStreamFormat(QtIOCompressor::GzipFormat);
            if (compressor.open(QIODevice::WriteOnly)) {
                compressor.write(resp.toUtf8().constData());
            }
        }
    }
    setHtml(str, Page_Information);
}

void SongView::abortInfoSearch()
{
    if (cancelInfoJobAction->isEnabled()) {
        cancelInfoJobAction->setEnabled(false);
        engine->cancel();
        hideSpinner();
    }
}

void SongView::showMoreInfo(const QUrl &url)
{
    QDesktopServices::openUrl(url);
}

void SongView::hideSpinner()
{
    if (!cancelInfoJobAction->isEnabled() && !cancelJobAction->isEnabled()) {
        View::hideSpinner();
    }
}

void SongView::abort()
{
    if (job) {
        job->cancelAndDelete();
        job=0;
    }
    currentProvider=-1;
    if (currentProv) {
        currentProv->abort();
        currentProv=0;

        text->setText(QString());
        // Set lyrics file anyway - so that editing is enabled!
        lyricsFile=Settings::self()->storeLyricsInMpdDir() && !currentSong.isNonMPD()
                ? mpdLyricsFilePath(currentSong)
                : lyricsCacheFileName(currentSong);
        setMode(Mode_Display);
    }
    cancelJobAction->setEnabled(false);
    hideSpinner();
}

void SongView::update(const Song &s, bool force)
{
    if (s.isEmpty() || s.title.isEmpty() || s.artist.isEmpty()) {
        currentSong=s;
        infoNeedsUpdating=tagsNeedsUpdating=lyricsNeedsUpdating=false;
        clear();
        abort();
        return;
    }

    Song song(s);
    bool songChanged = song.artist!=currentSong.artist || song.title!=currentSong.title;

    if (songChanged) {
        abort();
    }
    if (!isVisible()) {
        if (songChanged) {
            needToUpdate=true;
        }
        currentSong=song;
        return;
    }

    if (force || songChanged) {
        setMode(Mode_Blank);
//        controls->setVisible(true);
        currentRequest++;
        currentSong=song;

        if (song.title.isEmpty() || song.artist.isEmpty()) {
            clear();
            return;
        }

        // Only reset the provider if the refresh was an automatic one or if the song has
        // changed. Otherwise we'll keep the provider so the user can cycle through the lyrics
        // offered by the various providers.
        if (!force || songChanged) {
            currentProvider=-1;
        }

        infoNeedsUpdating=tagsNeedsUpdating=lyricsNeedsUpdating=true;
        setHeader(song.title);
        curentViewChanged();
    }
}

void SongView::downloadFinished()
{
    NetworkJob *reply=qobject_cast<NetworkJob *>(sender());
    if (reply) {
        reply->deleteLater();
        if (job==reply) {
            if (reply->ok()) {
                QString file=reply->property("file").toString();
                if (!file.isEmpty() && file==currentSong.file) {
                    QTextStream str(reply->actualJob());
                    QString lyrics=str.readAll();
                    if (!lyrics.isEmpty()) {
                        text->setText(fixNewLines(lyrics));
                        cancelJobAction->setEnabled(false);
                        hideSpinner();
                        return;
                    }
                }
            }
            job=0;
        }
    }
    getLyrics();
}

void SongView::lyricsReady(int id, QString lyrics)
{
    if (id != currentRequest) {
        return;
    }
    lyrics=lyrics.trimmed();

    if (lyrics.isEmpty()) {
        getLyrics();
    } else {
        cancelJobAction->setEnabled(false);
        hideSpinner();
        QString before=text->toHtml();
        text->setText(fixNewLines(lyrics));
        // Remove formatting, as we dont save this anyway...
        QString plain=text->toPlainText().trimmed();

        if (plain.isEmpty()) {
            text->setText(before);
            getLyrics();
        } else {
            text->setText(fixNewLines(plain));
            lyricsFile=QString();
            if (! ( Settings::self()->storeLyricsInMpdDir() && !currentSong.isNonMPD() &&
                    saveFile(mpdLyricsFilePath(currentSong))) ) {
                saveFile(lyricsCacheFileName(currentSong, true));
            }
            setMode(Mode_Display);
        }
    }
}

bool SongView::saveFile(const QString &fileName)
{
    QFile f(fileName);

    if (f.open(QIODevice::WriteOnly)) {
        QTextStream(&f) << text->toPlainText();
        f.close();
        lyricsFile=fileName;
        return true;
    }

    return false;
}

QString SongView::mpdFileName() const
{
    return currentSong.file.isEmpty() || MPDConnection::self()->getDetails().dir.isEmpty() || currentSong.isNonMPD()
            ? QString() : mpdLyricsFilePath(currentSong);
}

QString SongView::cacheFileName() const
{
    return currentSong.artist.isEmpty() || currentSong.title.isEmpty() ? QString() : lyricsCacheFileName(currentSong);
}

void SongView::getLyrics()
{
    currentProv=UltimateLyrics::self()->getNext(currentProvider);
    if (currentProv) {
        text->setText(i18n("Fetching lyrics via %1", currentProv->displayName()));
        currentProv->fetchInfo(currentRequest, currentSong);
        showSpinner();
    } else {
        text->setText(QString());
        currentProvider=-1;
        // Set lyrics file anyway - so that editing is enabled!
        lyricsFile=Settings::self()->storeLyricsInMpdDir() && !currentSong.isNonMPD()
                ? mpdLyricsFilePath(currentSong)
                : lyricsCacheFileName(currentSong);
        setMode(Mode_Display);
    }
}

void SongView::setMode(Mode m)
{
    if (Mode_Display==m) {
        cancelJobAction->setEnabled(false);
        hideSpinner();
    }
    if (mode==m) {
        return;
    }
    mode=m;
    bool fileExists=Mode_Display==m && !lyricsFile.isEmpty() && QFileInfo(lyricsFile).isWritable();
    delAction->setEnabled(fileExists);
    refreshAction->setEnabled(fileExists);
    if (scrollAction->isChecked()) {
        if (Mode_Display==mode) {
            scroll();
        } else if (scrollTimer) {
            scrollTimer->stop();
        }
    }
}

bool SongView::setLyricsFromFile(const QString &filePath)
{
    QFile f(filePath);

    if (f.exists() && f.open(QIODevice::ReadOnly)) {
        // Read the file using a QTextStream so we get automatic UTF8 detection.
        QTextStream inputStream(&f);

        text->setText(fixNewLines(inputStream.readAll()));
        cancelJobAction->setEnabled(false);
        hideSpinner();
        f.close();

        return true;
    }

    return false;
}
