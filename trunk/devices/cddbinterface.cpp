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

#include "cddbinterface.h"
#include "settings.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KProtocolManager>
#else
#include "networkproxyfactory.h"
#endif
#include "localize.h"
#include "thread.h"
#include <QNetworkProxy>
#include <QSet>
#include <cddb/cddb.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#include <sys/cdio.h>
#include <arpa/inet.h>
#elif defined(__linux__)
#include <linux/cdrom.h>
#endif

static struct CddbInterfaceCleanup
{
    ~CddbInterfaceCleanup() { libcddb_shutdown(); }
} cleanup;

QString CddbInterface::dataTrack()
{
    return i18n("Data Track");
}

CddbInterface::CddbInterface(const QString &device)
    : dev(device)
    , disc(0)
{
    thread=new Thread(metaObject()->className());
    moveToThread(thread);
    thread->start();
}

CddbInterface::~CddbInterface()
{
    thread->stop();
    if (disc) {
        cddb_disc_destroy(disc);
    }
}

static CdAlbum toAlbum(cddb_disc_t *disc, const CdAlbum &initial=CdAlbum())
{
    CdAlbum album;
    if (!disc) {
        return album;
    }
    album.name=QString::fromUtf8(cddb_disc_get_title(disc));
    album.artist=QString::fromUtf8(cddb_disc_get_artist(disc));
    album.genre=QString::fromUtf8(cddb_disc_get_genre(disc));
    album.year=cddb_disc_get_year(disc);
    int numTracks=cddb_disc_get_track_count(disc);
    if (numTracks>0) {
        for (int t=0; t<numTracks; ++t) {
            cddb_track_t *trk=cddb_disc_get_track(disc, t);
            if (!trk) {
                continue;
            }

            Song track;
            track.track=cddb_track_get_number(trk);
            track.title=QString::fromUtf8(cddb_track_get_title(trk));
            track.artist=QString::fromUtf8(cddb_track_get_artist(trk));
            track.albumartist=album.artist;
            track.album=album.name;
            track.id=track.track;
            track.file=QString("%1.wav").arg(track.track);
            track.year=album.year;

            if (initial.isNull()) {
                track.time=cddb_track_get_length(trk);
                if (CddbInterface::dataTrack()==track.title) {
                    // Adjust last track length...
                    if (album.tracks.count()) {
                        Song last=album.tracks.takeLast();
                        last.time-=FRAMES_TO_SECONDS(11400);
                        album.tracks.append(last);
                    }
                } else {
                    album.tracks.append(track);
                }
            } else if (t>=initial.tracks.count()) {
                break;
            } else {
                track.time=initial.tracks.at(t).time;
                album.tracks.append(track);
            }
        }
    }

    // Ensure we always have same number of tracks...
    if (!initial.isNull() && album.tracks.count()<initial.tracks.count()) {
        for (int i=album.tracks.count(); i<initial.tracks.count(); ++i) {
            album.tracks.append(initial.tracks.at(i));
        }
    }
    return album;
}

// Copied from asunder v2.2 - GPL v2
void CddbInterface::readDisc()
{
    if (disc) {
        return;
    }

    int fd=open(dev.toLocal8Bit(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        emit error(i18n("Failed to open CD device"));
        return;
    }
    QByteArray unknown=i18n("Unknown").toUtf8();

    #if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
    struct ioc_toc_header th;
    struct ioc_read_toc_single_entry te;
    struct ioc_read_subchannel cdsc;
    struct cd_sub_channel_info data;
    bzero(&cdsc, sizeof(cdsc));
    cdsc.data = &data;
    cdsc.data_len = sizeof(data);
    cdsc.data_format = CD_CURRENT_POSITION;
    cdsc.address_format = CD_MSF_FORMAT;
    if (ioctl(fd, CDIOCREADSUBCHANNEL, (char *)&cdsc) >= 0 && 0==ioctl(fd, CDIOREADTOCHEADER, &th)) {
        disc = cddb_disc_new();
        if (disc) {
            te.address_format = CD_LBA_FORMAT;
            for (int i=th.starting_track; i<=th.ending_track; i++) {
                te.track = i;
                if (0==ioctl(fd, CDIOREADTOCENTRY, &te)) {
                    cddb_track_t *track = cddb_track_new();
                    if (track) {
                        cddb_track_set_frame_offset(track, te.entry.addr.lba + SECONDS_TO_FRAMES(2));
                        cddb_track_set_title(track, te.entry.control&0x04 ? dataTrack().toUtf8().constData() : i18n("Track %1", i).toUtf8().constData());
                        cddb_track_set_artist(track, unknown.constData());
                        cddb_disc_add_track(disc, track);
                    }
                }
            }
            te.track = 0xAA;
            if (0==ioctl(fd, CDIOREADTOCENTRY, &te))  {
                cddb_disc_set_length(disc, (ntohl(te.entry.addr.lba)+SECONDS_TO_FRAMES(2))/SECONDS_TO_FRAMES(1));
            }
        }
    }
    #elif defined(__linux__)
    struct cdrom_tochdr th;
    struct cdrom_tocentry te;
    int status = ioctl(fd, CDROM_DISC_STATUS, CDSL_CURRENT);
    if ((CDS_AUDIO==status || CDS_MIXED==status) && 0==ioctl(fd, CDROMREADTOCHDR, &th)) {
        disc = cddb_disc_new();
        if (disc) {
            te.cdte_format = CDROM_LBA;
            for (int i=th.cdth_trk0; i<=th.cdth_trk1; i++) {
                te.cdte_track = i;
                if (0==ioctl(fd, CDROMREADTOCENTRY, &te)) {
                    cddb_track_t *track = cddb_track_new();
                    if (track) {
                        cddb_track_set_frame_offset(track, te.cdte_addr.lba + SECONDS_TO_FRAMES(2));
                        cddb_track_set_title(track, te.cdte_ctrl&CDROM_DATA_TRACK ? dataTrack().toUtf8().constData() : i18n("Track %1", i).toUtf8().constData());
                        cddb_track_set_artist(track, unknown.constData());
                        cddb_disc_add_track(disc, track);
                    }
                }
            }

            te.cdte_track = CDROM_LEADOUT;
            if (0==ioctl(fd, CDROMREADTOCENTRY, &te)) {
                cddb_disc_set_length(disc, (te.cdte_addr.lba+SECONDS_TO_FRAMES(2))/SECONDS_TO_FRAMES(1));
            }
        }
    }
    #endif

    if (disc) {
        cddb_disc_set_artist(disc, unknown.constData());
        cddb_disc_set_title(disc, unknown.constData());
        cddb_disc_set_genre(disc, unknown.constData());
        cddb_disc_calc_discid(disc);
    }
    close(fd);

    initial=toAlbum(disc);
    initial.isDefault=true;
    emit initialDetails(initial);
}

class CddbConnection
{
public:
    CddbConnection(cddb_disc_t *d) : disc(0) {
        connection = cddb_new();
        if (connection) {
            cddb_cache_disable(connection);
            cddb_set_server_name(connection, Settings::self()->cddbHost().toLatin1().constData());
            cddb_set_server_port(connection, Settings::self()->cddbPort());
            disc=cddb_disc_clone(d);
            #ifdef ENABLE_KDE_SUPPORT
            QString proxy=KProtocolManager::proxyFor("http");
            if (!proxy.isEmpty()) {
                QUrl url(proxy);
                cddb_set_http_proxy_server_name(connection, url.host().toLatin1().constData());
                cddb_set_http_proxy_server_port(connection, url.port());
                cddb_http_proxy_enable(connection);
            }
            #else
            QUrl url;
            url.setHost(Settings::self()->cddbHost());
            url.setPort(Settings::self()->cddbPort());
            QList<QNetworkProxy> proxies=NetworkProxyFactory::self()->queryProxy(QNetworkProxyQuery(url));

            foreach (const QNetworkProxy &p, proxies) {
                if (QNetworkProxy::HttpProxy==p.type() && 0!=p.port()) {
                    cddb_set_http_proxy_server_name(connection, p.hostName().toLatin1().constData());
                    cddb_set_http_proxy_server_port(connection, p.port());
                    cddb_http_proxy_enable(connection);
                    break;
                }
            }
            #endif
        }
    }

    ~CddbConnection() {
        if (disc) {
            cddb_disc_destroy(disc);
        }
        if (connection) {
            cddb_destroy(connection);
        }
    }

    operator bool() const { return 0!=connection; }
    int query() { return cddb_query(connection, disc); }
    int read() { return cddb_read(connection, disc); }
    const char * error() { return cddb_error_str(cddb_errno(connection)); }
    int trackCount() {  return cddb_disc_get_track_count(disc); }
    int next() { return cddb_query_next(connection, disc); }
    CdAlbum toAlbum(const CdAlbum &initial) { return ::toAlbum(disc, initial); }

private:
    cddb_conn_t *connection;
    cddb_disc_t *disc;
};

void CddbInterface::lookup(bool full)
{
    bool isInitial=!disc;
    if (!disc) {
        readDisc();
    }

    if (!disc || !full) {
        // Errors already logged in readDisc
        return;
    }

    CddbConnection cddb(disc);
    if (!cddb) {
        emit error(i18n("Failed to create CDDB connection"));
        return;
    }

    if (cddb.query()<1) {
        if (!isInitial) {
            emit error(i18n("No matches found in CDDB"));
        }
        return;
    }

    QList<CdAlbum> m;
    for (;;) {
        if (!cddb.read()) {
            emit error(i18n("CDDB error: %1", cddb.error()));
            return;
        }
        int numTracks=cddb.trackCount();
        if (numTracks<=0) {
            continue;
        }

        CdAlbum album=cddb.toAlbum(initial);
        if (!album.tracks.isEmpty()) {
            m.append(album);
        }
        if (!cddb.next()) {
            break;
        }
    }

    if (m.isEmpty()) {
        if (!isInitial) {
            emit error(i18n("No matches found in CDDB"));
        }
    } else if (isInitial) {
        emit initialDetails(m.first());
    } else {
        emit matches(m);
    }
}
