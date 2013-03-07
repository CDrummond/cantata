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

#include "audiocddevice.h"
#include "cddb.h"
#include "localize.h"
#include "musiclibraryitemsong.h"
#include "musiclibrarymodel.h"
#include "dirviewmodel.h"
#include "utils.h"
#include "extractjob.h"
#include "mpdconnection.h"
#include "covers.h"
#include <QDir>

QString AudioCdDevice::coverUrl(QString udi)
{
    udi.replace(" ", "_");
    udi.replace("\n", "_");
    udi.replace("\t", "_");
    udi.replace("/", "_");
    udi.replace(":", "_");
    return QLatin1String("cdda://")+udi;
}

AudioCdDevice::AudioCdDevice(DevicesModel *m, Solid::Device &dev)
    : Device(m, dev, false, true)
    , cddb(0)
    , year(0)
    , disc(0)
    , time(0xFFFFFFFF)
    , lookupInProcess(false)
{
    drive=dev.parent().as<Solid::OpticalDrive>();
    block=dev.as<Solid::Block>();
    if (block) {
        cddb=new Cddb(block->device());
        connect(cddb, SIGNAL(error(QString)), this, SIGNAL(error(QString)));
        connect(cddb, SIGNAL(initialDetails(CddbAlbum)), this, SLOT(setDetails(CddbAlbum)));
        connect(this, SIGNAL(lookup()), cddb, SLOT(lookup()));
        connect(cddb, SIGNAL(matches(const QList<CddbAlbum> &)), SLOT(cddbMatches(const QList<CddbAlbum> &)));
        detailsString=i18n("Reading disc");
        setStatusMessage(detailsString);
        lookupInProcess=true;
        connect(Covers::self(), SIGNAL(cover(const Song &, const QImage &, const QString &)),
                this, SLOT(setCover(const Song &, const QImage &, const QString &)));
        emit lookup();
    }
}

AudioCdDevice::~AudioCdDevice()
{
}

void AudioCdDevice::rescan(bool)
{
    if (cddb) {
        lookupInProcess=true;
        emit lookup();
    }
}

void AudioCdDevice::toggle()
{
    if (drive) {
        stop();
        drive->eject();
    }
}

void AudioCdDevice::stop()
{
}

void AudioCdDevice::copySongTo(const Song &s, const QString &baseDir, const QString &musicPath, bool overwrite, bool copyCover)
{
    jobAbortRequested=false;
    if (!isConnected()) {
        emit actionStatus(NotConnected);
        return;
    }

    needToFixVa=opts.fixVariousArtists && s.isVariousArtists();

    if (!overwrite) {
        Song check=s;

        if (needToFixVa) {
            Device::fixVariousArtists(QString(), check, false);
        }
        if (MusicLibraryModel::self()->songExists(check)) {
            emit actionStatus(SongExists);
            return;
        }
    }

    DeviceOptions mpdOpts;
    mpdOpts.load(MPDConnectionDetails::configGroupName(MPDConnection::self()->getDetails().name), true);

    encoder=Encoders::getEncoder(mpdOpts.transcoderCodec);
    if (encoder.codec.isEmpty()) {
        emit actionStatus(CodecNotAvailable);
        return;
    }


    QString source=block->device();
    currentBaseDir=baseDir;
    currentMusicPath=musicPath;
    QString dest(currentBaseDir+currentMusicPath);
    dest=encoder.changeExtension(dest);
    QDir dir(Utils::getDir(dest));
    if (!dir.exists() && !Utils::createDir(dir.absolutePath(), baseDir)) {
        emit actionStatus(DirCreationFaild);
        return;
    }

    currentSong=s;
    ExtractJob *job=new ExtractJob(encoder, mpdOpts.transcoderValue, source, dest, currentSong, copyCover ? coverImage.fileName : QString());
    connect(job, SIGNAL(result(int)), SLOT(copySongToResult(int)));
    connect(job, SIGNAL(percent(int)), SLOT(percent(int)));
    job->start();
}

quint32 AudioCdDevice::totalTime()
{
    if (0xFFFFFFFF==time) {
        time=0;
        foreach (MusicLibraryItem *i, childItems()) {
            time+=static_cast<MusicLibraryItemSong *>(i)->song().time;
        }
    }

    return time;
}

void AudioCdDevice::percent(int pc)
{
    if (jobAbortRequested && 100!=pc) {
        FileJob *job=qobject_cast<FileJob *>(sender());
        if (job) {
            job->stop();
        }
        return;
    }
    emit progress(pc);
}

void AudioCdDevice::copySongToResult(int status)
{
    ExtractJob *job=qobject_cast<ExtractJob *>(sender());
    FileJob::finished(job);
    if (jobAbortRequested) {
        if (job && job->wasStarted() && QFile::exists(currentBaseDir+currentMusicPath)) {
            QFile::remove(currentBaseDir+currentMusicPath);
        }
        return;
    }
    if (Ok!=status) {
        emit actionStatus(status);
    } else {
        currentSong.file=currentMusicPath; // MPD's paths are not full!!!
        if (needToFixVa) {
            currentSong.revertVariousArtists();
        }
        Utils::setFilePerms(currentBaseDir+currentSong.file);
        MusicLibraryModel::self()->addSongToList(currentSong);
        DirViewModel::self()->addFileToList(currentSong.file);
        emit actionStatus(Ok, job && job->coverCopied());
    }
}

void AudioCdDevice::setDetails(const CddbAlbum &a)
{
    bool differentAlbum=album!=a.name || artist!=a.artist;
    lookupInProcess=false;
    setData(a.artist);
    album=a.name;
    artist=a.artist;
    genre=a.genre;
    year=a.year;
    disc=a.disc;
    update=new MusicLibraryItemRoot();
    int totalDuration=0;
    foreach (const Song &s, a.tracks) {
        totalDuration+=s.time;
        update->append(new MusicLibraryItemSong(s, update));
    }
    setStatusMessage(QString());
    #ifdef ENABLE_KDE_SUPPORT
    detailsString=i18np("1 Track (%2)", "%1 Tracks (%2)", a.tracks.count(), Song::formattedTime(totalDuration));
    #else
    detailsString=QTP_TRACKS_DURATION_STR(a.tracks.count(), Song::formattedTime(totalDuration));
    #endif
    emit updating(udi(), false);
    if (differentAlbum) {
        Song s;
        s.artist=artist;
        s.album=album;
        s.file=AudioCdDevice::coverUrl(udi());
        Covers::self()->requestCover(s);
    }
}

void AudioCdDevice::cddbMatches(const QList<CddbAlbum> &albums)
{
    lookupInProcess=false;
    if (1==albums.count()) {
        setDetails(albums.at(0));
    } else if (albums.count()>1) {
        // More than 1 match, so prompt user!
        emit matches(udi(), albums);
    }
}

void AudioCdDevice::setCover(const Song &song, const QImage &img, const QString &file)
{
    if(song.file.startsWith("cdda://") && song.artist==artist && song.album==album) {
        coverImage=Covers::Image(img, file);
        updateStatus();
    }
}
