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

#include "songinfoprovider.h"
#include "lyricspage.h"
#include "lyricsdialog.h"
#include "ultimatelyricsprovider.h"
#include "ultimatelyricsreader.h"
#include "network.h"
#include "settings.h"
#include "mainwindow.h"
#include "squeezedtextlabel.h"
#include "utils.h"
#include "messagebox.h"
#include "localize.h"
#ifdef TAGLIB_FOUND
#include "tags.h"
#endif
#include "icon.h"
#include "utils.h"
#include <QtCore/QFuture>
#include <QtCore/QFutureWatcher>
#include <QtCore/QSettings>
#include <QtCore/QtConcurrentRun>
#include <QTextBrowser>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtGui/QToolButton>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KAction>
#include <KDE/KXMLGUIClient>
#include <KDE/KActionCollection>
#endif

static const QLatin1String constLyricsDir("lyrics/");
const QLatin1String LyricsPage::constExtension(".lyrics");

static QString cacheFile(QString artist, QString title, bool createDir=false)
{
    title.replace("/", "_");
    artist.replace("/", "_");
    return QDir::toNativeSeparators(Network::cacheDir(constLyricsDir+artist+'/', createDir))+title+LyricsPage::constExtension;
}

typedef QList<UltimateLyricsProvider *> ProviderList;

bool CompareLyricProviders(const UltimateLyricsProvider* a, const UltimateLyricsProvider* b) {
    return a->relevance() < b->relevance();
}

LyricsPage::LyricsPage(MainWindow *p)
    : QWidget(p)
//     , reader(new UltimateLyricsReader(this))
    , currentRequest(0)
    , mode(Mode_Display)
{
    setupUi(this);

    providers=UltimateLyricsReader().Parse(QString(":lyrics.xml"));
    foreach (UltimateLyricsProvider* provider, providers) {
        connect(provider, SIGNAL(InfoReady(int, const QString &)), SLOT(resultReady(int, const QString &)));
    }

    // Parse the ultimate lyrics xml file in the background
//     QFuture<ProviderList> future = QtConcurrent::run(reader.data(), &UltimateLyricsReader::Parse,
//                                                      QString(":lyrics.xml"));
//     QFutureWatcher<ProviderList> *watcher = new QFutureWatcher<ProviderList>(this);
//     watcher->setFuture(future);
//     connect(watcher, SIGNAL(finished()), SLOT(ultimateLyricsParsed()));
    #ifdef ENABLE_KDE_SUPPORT
    refreshAction = p->actionCollection()->addAction("refreshlyrics");
    refreshAction->setText(i18n("Refresh"));
    searchAction = p->actionCollection()->addAction("searchlyrics");
    searchAction->setText(i18n("Search For Lyrics"));
    editAction = p->actionCollection()->addAction("editlyrics");
    editAction->setText(i18n("Edit Lyrics"));
    saveAction = p->actionCollection()->addAction("savelyrics");
    saveAction->setText(i18n("Save Lyrics"));
    cancelAction = p->actionCollection()->addAction("canceleditlyrics");
    cancelAction->setText(i18n("Cancel Editing Lyrics"));
    delAction = p->actionCollection()->addAction("dellyrics");
    delAction->setText(i18n("Delete Lyrics File"));
    #else
    refreshAction = new QAction(tr("Refresh"), this);
    searchAction = new QAction(tr("Search For Lyrics"), this);
    editAction = new QAction(tr("Refresh"), this);
    saveAction = new QAction(tr("Refresh"), this);
    cancelAction = new QAction(tr("Refresh"), this);
    delAction = new QAction(tr("Refresh"), this);
    #endif
    refreshAction->setIcon(Icon("view-refresh"));
    searchAction->setIcon(Icon("edit-find"));
    editAction->setIcon(Icon("document-edit"));
    saveAction->setIcon(Icon("document-save"));
    cancelAction->setIcon(Icon("dialog-cancel"));
    delAction->setIcon(Icon("edit-delete"));
    connect(refreshAction, SIGNAL(triggered()), SLOT(update()));
    connect(searchAction, SIGNAL(triggered()), SLOT(search()));
    connect(editAction, SIGNAL(triggered()), SLOT(edit()));
    connect(saveAction, SIGNAL(triggered()), SLOT(save()));
    connect(cancelAction, SIGNAL(triggered()), SLOT(cancel()));
    connect(delAction, SIGNAL(triggered()), SLOT(del()));
    Icon::init(refreshBtn);
    Icon::init(searchBtn);
    Icon::init(editBtn);
    Icon::init(saveBtn);
    Icon::init(cancelBtn);
    Icon::init(delBtn);
    refreshBtn->setDefaultAction(refreshAction);
    searchBtn->setDefaultAction(searchAction);
    editBtn->setDefaultAction(editAction);
    saveBtn->setDefaultAction(saveAction);
    cancelBtn->setDefaultAction(cancelAction);
    delBtn->setDefaultAction(delAction);
    #ifdef CANTATA_ANDROID
    setBgndImageEnabled(false);
    #else
    setBgndImageEnabled(Settings::self()->lyricsBgnd());
    text->setZoom(Settings::self()->lyricsZoom());
    #endif
    setMode(Mode_Blank);
}

LyricsPage::~LyricsPage()
{
    foreach (UltimateLyricsProvider* provider, providers) {
        delete provider;
    }
}

void LyricsPage::saveSettings()
{
    Settings::self()->saveLyricsZoom(text->zoom());
}

void LyricsPage::setEnabledProviders(const QStringList &providerList)
{
    foreach (UltimateLyricsProvider* provider, providers) {
        provider->set_enabled(false);
        provider->set_relevance(0xFFFF);
    }

    int relevance=0;
    foreach (const QString &p, providerList) {
        UltimateLyricsProvider *provider=providerByName(p);
        if (provider) {
            provider->set_enabled(true);
            provider->set_relevance(relevance++);
        }
    }

    qSort(providers.begin(), providers.end(), CompareLyricProviders);
}

void LyricsPage::update()
{
    QString mpdName=mpdFileName();
    bool mpdExists=!mpdName.isEmpty() && QFile::exists(mpdName);
    if (Mode_Edit==mode && MessageBox::No==MessageBox::warningYesNo(this, i18n("Abort editing of lyrics?"))) {
        return;
    } else if(mpdExists && MessageBox::No==MessageBox::warningYesNo(this, i18n("Delete saved copy of lyrics, and re-download?"))) {
        return;
    }
    if (mpdExists) {
        QFile::remove(mpdName);
    }
    QString cacheName=cacheFileName();
    if (!cacheName.isEmpty() && QFile::exists(cacheName)) {
        QFile::remove(cacheName);
    }
    update(currentSong, true);
}

void LyricsPage::search()
{
    if (Mode_Edit==mode && MessageBox::No==MessageBox::warningYesNo(this, i18n("Abort editing of lyrics?"))) {
        return;
    }
    setMode(Mode_Display);

    Song song=currentSong;
    LyricsDialog dlg(currentSong, this);
    if (QDialog::Accepted==dlg.exec()) {
        if ((song.artist!=currentSong.artist || song.title!=currentSong.title) &&
                MessageBox::No==MessageBox::warningYesNo(this, i18n("Current playing song has changed, still perform search?"))) {
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

void LyricsPage::edit()
{
    preEdit=text->toPlainText();
    setMode(Mode_Edit);
}

void LyricsPage::save()
{
    if (preEdit!=text->toPlainText()) {
        if (MessageBox::No==MessageBox::warningYesNo(this, i18n("Save updated lyrics?"))) {
            return;
        }

        QString mpdName=mpdFileName();
        QString cacheName=cacheFileName();
        if (!mpdName.isEmpty() && saveFile(mpdName)) {
            if (QFile::exists(cacheName)) {
                QFile::remove(cacheName);
            }
            Utils::setFilePerms(mpdName);
        } else if (cacheName.isEmpty() || !saveFile(cacheName)) {
            MessageBox::error(this, i18n("Failed to save lyrics."));
            return;
        }
    }

    setMode(Mode_Display);
}

void LyricsPage::cancel()
{
    if (preEdit!=text->toPlainText()) {
        if (MessageBox::No==MessageBox::warningYesNo(this, i18n("Abort editing of lyrics?"))) {
            return;
        }
    }
    text->setText(preEdit);
    setMode(Mode_Display);
}

void LyricsPage::del()
{
    if (MessageBox::No==MessageBox::warningYesNo(this, i18n("Delete lyrics file?"))) {
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

void LyricsPage::update(const Song &s, bool force)
{
    if (Mode_Edit==mode && !force) {
        return;
    }

    Song song(s);
    if (song.isVariousArtists()) {
        song.revertVariousArtists();
    }

    bool songChanged = song.artist!=currentSong.artist || song.title!=currentSong.title;

    if (force || songChanged) {
        setMode(Mode_Blank);
        controls->setVisible(true);
        currentRequest++;
        currentSong=song;

        if (song.title.isEmpty() || song.artist.isEmpty()) {
            text->setText(QString());
            return;
        }

        // Only reset the provider if the refresh
        // was an automatic one or if the song has
        // changed. Otherwise we'll keep the provider
        // so the user can cycle through the lyrics
        // offered by the various providers.
        if (!force || songChanged) {
            currentProvider=-1;
        }

        if (!MPDConnection::self()->getDetails().dir.isEmpty() && !song.file.isEmpty() && !song.isStream()) {
            QString songFile=song.file;

            if (song.isCantataStream()) {
                QUrl u(songFile);
                songFile=u.hasQueryItem("file") ? u.queryItemValue("file") : QString();
            }

            #ifdef TAGLIB_FOUND
            QString tagLyrics=Tags::readLyrics(MPDConnection::self()->getDetails().dir+songFile);

            if (!tagLyrics.isEmpty()) {
                text->setText(tagLyrics);
                setMode(Mode_Display);
                controls->setVisible(false);
                return;
            }
            #endif

            // Check for MPD file...
            QString mpdLyrics=Utils::changeExtension(MPDConnection::self()->getDetails().dir+songFile, constExtension);

//             if (force && QFile::exists(mpdLyrics)) {
//                 QFile::remove(mpdLyrics);
//             }

            // Stop here if we found lyrics in the cache dir.
            if (setLyricsFromFile(mpdLyrics)) {
                lyricsFile=mpdLyrics;
                setMode(Mode_Display);
                return;
            }
        }

        // Check for cached file...
        QString file=cacheFile(song.artist, song.title);

        /*if (force && QFile::exists(file)) {
            // Delete the cached lyrics file when the user
            // is force-fully re-fetching the lyrics.
            // Afterwards we'll simply do getLyrics() to get
            // the new ones.
            QFile::remove(file);
        } else */if (setLyricsFromFile(file)) {
           // We just wanted a normal update without
           // explicit re-fetching. We can return
           // here because we got cached lyrics and
           // we don't want an explicit re-fetch.
           lyricsFile=file;
           setMode(Mode_Display);
           return;
        }

        getLyrics();
    }
}

void LyricsPage::resultReady(int id, const QString &lyrics)
{
    if (id != currentRequest)
        return;

    if (lyrics.isEmpty()) {
        getLyrics();
    } else {
        text->setText(lyrics);
        // Remove formatting, as we dont save this anyway...
        text->setText(text->toPlainText());
        lyricsFile=QString();
        if (! ( Settings::self()->storeLyricsInMpdDir() &&
                saveFile(Utils::changeExtension(MPDConnection::self()->getDetails().dir+currentSong.file, constExtension))) ) {
            saveFile(cacheFile(currentSong.artist, currentSong.title, true));
        }
        setMode(Mode_Display);
    }
}

bool LyricsPage::saveFile(const QString &fileName)
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

QString LyricsPage::mpdFileName() const
{
    return currentSong.file.isEmpty() || MPDConnection::self()->getDetails().dir.isEmpty() || currentSong.isStream()
            ? QString() : Utils::changeExtension(MPDConnection::self()->getDetails().dir+currentSong.file, constExtension);
}

QString LyricsPage::cacheFileName() const
{
    return currentSong.artist.isEmpty() || currentSong.title.isEmpty() ? QString() : cacheFile(currentSong.artist, currentSong.title);
}

UltimateLyricsProvider* LyricsPage::providerByName(const QString &name) const
{
    foreach (UltimateLyricsProvider* provider, providers) {
        if (provider->name() == name) {
            return provider;
        }
    }
    return 0;
}

void LyricsPage::getLyrics()
{
    for(;;) {
        currentProvider++;
        if (currentProvider<providers.count()) {
            UltimateLyricsProvider *prov=providers.at(currentProvider++);
            if (prov && prov->is_enabled()) {
                #ifdef ENABLE_KDE_SUPPORT
                text->setText(i18nc("<title> by <artist>\nFetching lyrics via <url>", "%1 by %2\nFetching lyrics via %3",
                                    currentSong.title, currentSong.artist, prov->name()));
                #else
                text->setText(tr("%1 by %2\nFetching lyrics via %3").arg(currentSong.title).arg(currentSong.artist).arg(prov->name()));
                #endif
                prov->FetchInfo(currentRequest, currentSong);
                return;
            }
        } else {
            #ifdef ENABLE_KDE_SUPPORT
            text->setText(i18nc("<title> by <artist>\nFailed\n", "%1 by %2\nFailed to fetch lyrics", currentSong.title, currentSong.artist));
            #else
            text->setText(tr("%1 by %2\nFailed to fetch lyrics").arg(currentSong.title).arg(currentSong.artist));
            #endif
            currentProvider=-1;
            // Set lyrics file anyway - so that editing is enabled!
            lyricsFile=Settings::self()->storeLyricsInMpdDir()
                        ? Utils::changeExtension(MPDConnection::self()->getDetails().dir+currentSong.file, constExtension)
                        : cacheFile(currentSong.artist, currentSong.title);
            setMode(Mode_Display);
            return;
        }
    }
}

void LyricsPage::setMode(Mode m)
{
    if (mode==m) {
        return;
    }
    mode=m;
    bool editable=Mode_Display==m && !lyricsFile.isEmpty() && (!QFile::exists(lyricsFile) || QFileInfo(lyricsFile).isWritable());
    saveAction->setEnabled(Mode_Edit==m);
    cancelAction->setEnabled(Mode_Edit==m);
    editAction->setEnabled(editable);
    delAction->setEnabled(editable && !MPDConnection::self()->getDetails().dir.isEmpty() && QFile::exists(Utils::changeExtension(MPDConnection::self()->getDetails().dir+currentSong.file, constExtension)));
    text->setReadOnly(Mode_Edit!=m);
    songLabel->setVisible(Mode_Edit==m);
    #ifdef ENABLE_KDE_SUPPORT
    songLabel->setText(Mode_Edit==m ? i18nc("title, by artist", "%1, by %2", currentSong.title, currentSong.artist) : QString());
    #else
    songLabel->setText(Mode_Edit==m ? tr("%1, by %2").arg(currentSong.title).arg(currentSong.artist) : QString());
    #endif
}

bool LyricsPage::setLyricsFromFile(const QString &filePath) const
{
    bool success = false;

    QFile f(filePath);

    if (f.exists() && f.open(QIODevice::ReadOnly)) {
        // Read the file using a QTextStream so we get
        // automatic UTF8 detection.
        QTextStream inputStream(&f);

        text->setText(inputStream.readAll());
        f.close();

        success = true;
    }

    return success;
}

// void LyricsPage::ultimateLyricsParsed()
// {
//     QFutureWatcher<ProviderList>* watcher = static_cast<QFutureWatcher<ProviderList>*>(sender());
//     QStringList names;
//
//     foreach (UltimateLyricsProvider* provider, watcher->result()) {
//         providers.append(provider);
//         connect(provider, SIGNAL(InfoReady(int, const QString &)), SLOT(resultReady(int, const QString &)));
//     }
//
//     watcher->deleteLater();
//     reader.reset();
//     emit providersUpdated();
// }
