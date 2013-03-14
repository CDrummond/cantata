/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/*
  Copyright (C) 2005-2007 Richard Lärkäng <nouseforaname@home.se>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.
*/

#include "musicbrainz.h"
#ifndef ENABLE_KDE_SUPPORT
#include "networkproxyfactory.h"
#endif
#include <QNetworkProxy>
#include <QCryptographicHash>
#include <cstdio>
#include <cstring>
#include <musicbrainz5/Query.h>
#include <musicbrainz5/Medium.h>
#include <musicbrainz5/Release.h>
#include <musicbrainz5/ReleaseGroup.h>
#include <musicbrainz5/Track.h>
#include <musicbrainz5/Recording.h>
#include <musicbrainz5/Disc.h>
#include <musicbrainz5/HTTPFetch.h>
#include <musicbrainz5/ArtistCredit.h>
#include <musicbrainz5/Artist.h>
#include <musicbrainz5/NameCredit.h>
#include <QThread>
#include <QList>
#include <QRegExp>
#include "config.h"
#include "utils.h"
#include "localize.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#include <sys/cdio.h>
#elif defined(__linux__)
#include <linux/cdrom.h>
#endif

#include <QDebug>
#define DBUG qDebug()

static const int constFramesPerSecond=75;
static const int constDataTrackAdjust=11400;

static inline int secondsToFrames(int s) {
    return constFramesPerSecond *s;
}

static inline int framesToSeconds(int f) {
    return (f/(constFramesPerSecond*1.0))+0.5;
}

struct Track {
    Track(int o=0, bool d=false) : offset(o), isData(d) {}
    int offset;
    bool isData;
};

static QString calculateDiscId(const QList<Track> &tracks)
{
    // Code based on libmusicbrainz/lib/diskid.cpp
    int numTracks = tracks.count()-1;
    QCryptographicHash sha(QCryptographicHash::Sha1);
    char temp[9];

    // FIXME How do I check that?
    int firstTrack = 1;
    int lastTrack = numTracks;

    sprintf(temp, "%02X", firstTrack);
    sha.addData(temp, strlen(temp));

    sprintf(temp, "%02X", lastTrack);
    sha.addData(temp, strlen(temp));

    for(int i = 0; i < 100; i++) {
        long offset;
        if (0==i) {
            offset = tracks[numTracks].offset;
        } else if (i <= numTracks) {
            offset = tracks[i-1].offset;
        } else {
            offset = 0;
        }

        sprintf(temp, "%08lX", offset);
        sha.addData(temp, strlen(temp));
    }

    QByteArray base64 = sha.result().toBase64();
    // '/' '+' and '=' replaced for MusicBrainz
    return QString::fromLatin1(base64).replace(QLatin1Char( '/' ), QLatin1String( "_" ))
                                      .replace(QLatin1Char( '+' ), QLatin1String( "." ))
                                      .replace(QLatin1Char( '=' ), QLatin1String( "-" ));
}

static QString artistFromCreditList(MusicBrainz5::CArtistCredit *artistCredit )
{
    QString artistName;
    MusicBrainz5::CNameCreditList *artistList=artistCredit->NameCreditList();

    if (artistList) {
        for (int i=0; i < artistList->NumItems(); i++) {
            MusicBrainz5::CNameCredit* name=artistList->Item(i);
            MusicBrainz5::CArtist* artist = name->Artist();

            if (!name->Name().empty()) {
                artistName += QString::fromUtf8(name->Name().c_str());
            } else {
                artistName += QString::fromUtf8(artist->Name().c_str());
            }
            artistName += QString::fromUtf8(name->JoinPhrase().c_str());
        }
    }

    return artistName;
}

MusicBrainz::MusicBrainz(const QString &device)
    : dev(device)
{
    thread=new QThread();
    moveToThread(thread);
    thread->start();
}

MusicBrainz::~MusicBrainz()
{
    Utils::stopThread(thread);
}

void MusicBrainz::readDisc()
{
    QList<Track> tracks;
    int fd=-1;
    int status=-1;
    #if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
    struct ioc_toc_header th;
    struct ioc_read_toc_single_entry te;
    struct ioc_read_subchannel cdsc;
    struct cd_sub_channel_info data;
    #elif defined(__linux__)
    struct cdrom_tochdr th;
    struct cdrom_tocentry te;
    #endif

    // open the device
    fd = open(dev.toLocal8Bit(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        emit error(i18n("Failed to open CD device"));
        return;
    }

    #if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
    // read disc status info
    bzero(&cdsc,sizeof(cdsc));
    cdsc.data = &data;
    cdsc.data_len = sizeof(data);
    cdsc.data_format = CD_CURRENT_POSITION;
    cdsc.address_format = CD_MSF_FORMAT;
    status = ioctl(fd, CDIOCREADSUBCHANNEL, (char *)&cdsc);
    if (status >= 0 && 0==ioctl(fd, CDIOREADTOCHEADER, &th)) {
        te.address_format = CD_LBA_FORMAT;
        for (int i=th.starting_track; i<=th.ending_track; i++) {
            te.track = i;
            if (0==ioctl(fd, CDIOREADTOCENTRY, &te)) {
                tracks.append(Track(te.cdte_addr.lba + secondsToFrames(2), te.cdte_ctrl&CDROM_DATA_TRACK));
            }
        }
        te.track = 0xAA;
        if (0==ioctl(fd, CDIOREADTOCENTRY, &te))  {
            tracks.append((ntohl(te.entry.addr.lba)+secondsToFrames(2))/secondsToFrames(1));
        }
    }
    #elif defined(__linux__)
    // read disc status info
    status = ioctl(fd, CDROM_DISC_STATUS, CDSL_CURRENT);
    if ( (CDS_AUDIO==status || CDS_MIXED==status) && 0==ioctl(fd, CDROMREADTOCHDR, &th)) {
        te.cdte_format = CDROM_LBA;
        for (int i=th.cdth_trk0; i<=th.cdth_trk1; i++) {
            te.cdte_track = i;
            if (0==ioctl(fd, CDROMREADTOCENTRY, &te)) {
                tracks.append(Track(te.cdte_addr.lba + secondsToFrames(2), te.cdte_ctrl&CDROM_DATA_TRACK));
            }
        }
        te.cdte_track = CDROM_LEADOUT;
        if (0==ioctl(fd, CDROMREADTOCENTRY, &te)) {
            tracks.append((te.cdte_addr.lba+secondsToFrames(2))/secondsToFrames(1));
        }
    }
    #endif
    close(fd);

    QString unknown(i18n("Unknown"));
    initial.name=unknown;
    initial.artist=unknown;
    initial.genre=unknown;

    if (tracks.count()>1) {
        for (int i=0; i<tracks.count()-1; ++i) {
            const Track &trk=tracks.at(i);
            if (trk.isData) {
                continue;
            }
            const Track &next=tracks.at(i+1);
            Song s;
            s.track=i+1;
            s.title=unknown;
            s.artist=unknown;
            s.albumartist=initial.artist;
            s.album=initial.name;
            s.id=s.track;
            s.time=framesToSeconds((next.offset-trk.offset)-(next.isData ? constDataTrackAdjust : 0));
            s.file=QString("%1.wav").arg(s.track);
            s.year=initial.year;
            initial.tracks.append(s);
        }

        if (tracks.count()>=3 && tracks.at(tracks.count()-2).isData) {
            tracks.takeLast();
            Track last=tracks.takeLast();
            last.offset-=constDataTrackAdjust;
            tracks.append(last);
        }
    }

    discId = calculateDiscId(tracks);
    emit initialDetails(initial);
}

void MusicBrainz::lookup(bool full)
{
    if (discId.isEmpty()) {
        readDisc();
    }

    if (!full) {
        return;
    }
    DBUG << "Should lookup " << discId;

    MusicBrainz5::CQuery Query("cantata-"PACKAGE_VERSION_STRING);
    QList<CdAlbum> m;

    #ifdef ENABLE_KDE_SUPPORT
    QNetworkProxy p(QNetworkProxy::DefaultProxy);
    if (QNetworkProxy::HttpProxy==p.type() && 0!=p.port()) {
        Query.SetProxyHost(p.hostName().toLatin1().constData());
        Query.SetProxyPort(p.port());
    }
    #else
    QList<QNetworkProxy> proxies=NetworkProxyFactory::self()->queryProxy(QNetworkProxyQuery(QUrl("http://musicbrainz.org")));
    foreach (const QNetworkProxy &p, proxies) {
        if (QNetworkProxy::HttpProxy==p.type() && 0!=p.port()) {
            Query.SetProxyHost(p.hostName().toLatin1().constData());
            Query.SetProxyPort(p.port());
            break;
        }
    }
    #endif
    // Code adapted from libmusicbrainz/examples/cdlookup.cc

    try {
        MusicBrainz5::CMetadata Metadata=Query.Query("discid",discId.toAscii().constData());

        if (Metadata.Disc() && Metadata.Disc()->ReleaseList()) {
            MusicBrainz5::CReleaseList *releaseList=Metadata.Disc()->ReleaseList();
            DBUG << "Found " << releaseList->NumItems() << " release(s)";

            for (int i = 0; i < releaseList->NumItems(); i++) {
                MusicBrainz5::CRelease* release=releaseList->Item(i);

                //The releases returned from LookupDiscID don't contain full information

                MusicBrainz5::CQuery::tParamMap params;
                params["inc"]="artists labels recordings release-groups url-rels discids artist-credits";

                std::string releaseId=release->ID();
                MusicBrainz5::CMetadata Metadata2=Query.Query("release", releaseId, "", params);

                if (Metadata2.Release()) {
                    MusicBrainz5::CRelease *fullRelease=Metadata2.Release();

                    //However, these releases will include information for all media in the release
                    //So we need to filter out the only the media we want.

                    MusicBrainz5::CMediumList mediaList=fullRelease->MediaMatchingDiscID(discId.toAscii().constData());

                    if (mediaList.NumItems() > 0) {
                        DBUG << "Found " << mediaList.NumItems() << " media item(s)";

                        for (int i=0; i < mediaList.NumItems(); i++) {
                            MusicBrainz5::CMedium* medium= mediaList.Item(i);

                            /*DBUG << "Found media: '" << medium.Title() << "', position " << medium.Position();*/
                            CdAlbum album;

                            album.name=QString::fromUtf8(fullRelease->Title().c_str());

                            if (fullRelease->MediumList()->NumItems() > 1) {
                                album.name = i18n("%1 (Disc %2)").arg(album.name).arg(medium->Position());
                                album.disc=medium->Position();
                            }
                            album.artist=artistFromCreditList(fullRelease->ArtistCredit());
                            album.genre=i18n("Unknown");

                            QString date = QString::fromUtf8(fullRelease->Date().c_str());
                            QRegExp yearRe("^(\\d{4,4})(-\\d{1,2}-\\d{1,2})?$");
                            if (yearRe.indexIn(date) > -1) {
                                QString yearString = yearRe.cap(1);
                                bool ok;
                                album.year=yearString.toInt(&ok);
                                if (!ok) {
                                    album.year = 0;
                                }
                            }

                            MusicBrainz5::CTrackList *trackList=medium->TrackList();
                            if (trackList) {
                                for (int i=0; i < trackList->NumItems(); i++) {
                                    // Ensure we have the same number of tracks are read from disc!
                                    if (album.tracks.count()>=initial.tracks.count()) {
                                        break;
                                    }
                                    MusicBrainz5::CTrack *track=trackList->Item(i);
                                    MusicBrainz5::CRecording *recording=track->Recording();
                                    Song song;

                                    song.albumartist=album.artist;
                                    song.album=album.name;
                                    song.genre=album.genre;
                                    song.id=song.track=track->Position();
                                    song.time=track->Length()/1000;
                                    song.disc=album.disc;
                                    song.file=QString("%1.wav").arg(song.track);

                                    // Prefer title and artist from the track credits, but it appears to be empty if same as in Recording
                                    // Noticable in the musicbrainztest-fulldate test, where the title on the credits of track 18 are
                                    // "Bara om min älskade väntar", but the recording has title "Men bara om min älskade"
                                    if (recording && 0==track->ArtistCredit()) {
                                        song.artist=artistFromCreditList(recording->ArtistCredit());
                                    } else {
                                        song.artist=artistFromCreditList(track->ArtistCredit());
                                    }

                                    if (recording && track->Title().empty()) {
                                        song.title=QString::fromUtf8(recording->Title().c_str());
                                    } else {
                                        song.title=QString::fromUtf8(track->Title().c_str());
                                    }
                                    album.tracks.append(song);
                                }
                            }

                            // Ensure we have the same number of tracks are read from disc!
                            if (album.tracks.count()<initial.tracks.count()) {
                                for (int i=album.tracks.count(); i<initial.tracks.count(); ++i) {
                                    album.tracks.append(initial.tracks.at(i));
                                }
                            }
                            m.append(album);
                        }
                    }
                }
            }
        }
    }
    catch (MusicBrainz5::CConnectionError &e) {
        DBUG << "MusicBrainz error" << e.what();
    }
    catch (MusicBrainz5::CTimeoutError &e) {
        DBUG << "MusicBrainz error - %1" << e.what();
    }
    catch (MusicBrainz5::CAuthenticationError &e) {
        DBUG << "MusicBrainz error - %1" << e.what();
    }
    catch (MusicBrainz5::CFetchError &e) {
        DBUG << "MusicBrainz error - %1" << e.what();
    }
    catch (MusicBrainz5::CRequestError &e) {
        DBUG << "MusicBrainz error - %1" << e.what();
    }
    catch (MusicBrainz5::CResourceNotFoundError &e) {
        DBUG << "MusicBrainz error - %1" << e.what();
    }

    if (m.isEmpty()) {
        emit error(i18n("No matches found in MusicBrainz"));
    } else {
        emit matches(m);
    }
}
