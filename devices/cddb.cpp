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

#include "cddb.h"
#include "settings.h"
#include "networkproxyfactory.h"
#include "localize.h"
#include "utils.h"
#include <QNetworkProxy>
#include <QSet>
#include <QThread>
#include <cddb/cddb.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#include <sys/cdio.h>
#elif defined(__linux__)
#include <linux/cdrom.h>
#endif

QString Cddb::dataTrack()
{
    return i18n("Data Track");
}

Cddb::Cddb(const QString &device)
    : dev(device)
    , disc(0)
{
    static bool registeredTypes=false;
    if (!registeredTypes) {
        qRegisterMetaType<CddbAlbum >("CddbAlbum");
        qRegisterMetaType<QList<CddbAlbum> >("QList<CddbAlbum>");
        registeredTypes=true;
    }

    thread=new QThread();
    moveToThread(thread);
    thread->start();
}

Cddb::~Cddb()
{
    Utils::stopThread(thread);
    if (disc) {
        cddb_disc_destroy(disc);
    }
}

static CddbAlbum toAlbum(cddb_disc_t *disc)
{
    CddbAlbum album;
    album.name=cddb_disc_get_title(disc);
    album.artist=cddb_disc_get_artist(disc);
    album.genre=cddb_disc_get_genre(disc);
    album.year=cddb_disc_get_year(disc);
    int numTracks=cddb_disc_get_track_count(disc);
    if (numTracks>0) {
        for (int t=0; t<numTracks; ++t) {
            cddb_track_t *trk=cddb_disc_get_track(disc, t);
            if (!trk) {
                continue;
            }

            Song track;
            track.id=track.track;
            track.track=cddb_track_get_number(trk);
            track.title=cddb_track_get_title(trk);
            track.artist=cddb_track_get_artist(trk);
            track.albumartist=album.artist;
            track.album=album.name;
            track.time=cddb_track_get_length(trk);
            track.file=QString("%1.wav").arg(track.track);
            track.year=album.year;
            if (Cddb::dataTrack()!=track.title) {
                album.tracks.append(track);
            }
        }
    }

    return album;
}

// Copied from asunder v2.2 - GPL v2
void Cddb::readDisc()
{
    if (disc) {
        return;
    }

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

    QByteArray unknown=i18n("Unknown").toLocal8Bit();

    #if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
    // read disc status info
    bzero(&cdsc,sizeof(cdsc));
    cdsc.data = &data;
    cdsc.data_len = sizeof(data);
    cdsc.data_format = CD_CURRENT_POSITION;
    cdsc.address_format = CD_MSF_FORMAT;
    status = ioctl(fd, CDIOCREADSUBCHANNEL, (char *)&cdsc);
    if (status >= 0 && 0==ioctl(fd, CDIOREADTOCHEADER, &th)) {
        disc = cddb_disc_new();
        if (!disc) {
            emit error(i18n("Failed read CD"));
            return 0;
        }
        te.address_format = CD_LBA_FORMAT;
        for (int i=th.starting_track; i<=th.ending_track; i++) {
            te.track = i;
            if (0==ioctl(fd, CDIOREADTOCENTRY, &te)) {
                cddb_track_t *track = cddb_track_new();
                if (!track) {
                    cddb_disc_destroy(disc);
                    disc=0;
                    emit error(i18n("Failed read CD track %1").arg(i));
                    return 0;
                }

                cddb_track_set_frame_offset(track, te.cdte_addr.lba + SECONDS_TO_FRAMES(2));
                cddb_track_set_title(track, (te.cdte_ctrl&CDROM_DATA_TRACK ? dataTrack() : i18n("Audio Track")).toLocal8Bit());
                cddb_track_set_artist(track, unknown.constData());
                cddb_disc_add_track(disc, track);
            }
        }
        te.track = 0xAA;
        if (0==ioctl(fd, CDIOREADTOCENTRY, &te))  {
            cddb_disc_set_length(disc, (ntohl(te.entry.addr.lba)+SECONDS_TO_FRAMES(2))/SECONDS_TO_FRAMES(1));
        }
    }
    #elif defined(__linux__)
    // read disc status info
    status = ioctl(fd, CDROM_DISC_STATUS, CDSL_CURRENT);
    if ( (CDS_AUDIO==status || CDS_MIXED==status) && 0==ioctl(fd, CDROMREADTOCHDR, &th)) {
        disc = cddb_disc_new();
        if (!disc) {
            emit error(i18n("Failed read CD"));
            return;
        }
        te.cdte_format = CDROM_LBA;
        for (int i=th.cdth_trk0; i<=th.cdth_trk1; i++) {
            te.cdte_track = i;
            if (ioctl(fd, CDROMREADTOCENTRY, &te) == 0) {
                cddb_track_t *track = cddb_track_new();
                if (!track) {
                    cddb_disc_destroy(disc);
                    disc=0;
                    emit error(i18n("Failed to read CD track %1").arg(i));
                    return;
                }

                cddb_track_set_frame_offset(track, te.cdte_addr.lba + SECONDS_TO_FRAMES(2));
                cddb_track_set_title(track, (te.cdte_ctrl&CDROM_DATA_TRACK ? dataTrack() : i18n("Audio Track")).toLocal8Bit());
                cddb_track_set_artist(track, unknown.constData());
                cddb_disc_add_track(disc, track);
            }
        }

        te.cdte_track = CDROM_LEADOUT;
        if (0==ioctl(fd, CDROMREADTOCENTRY, &te)) {
            cddb_disc_set_length(disc, (te.cdte_addr.lba+SECONDS_TO_FRAMES(2))/SECONDS_TO_FRAMES(1));
        }
    }
    #endif

    cddb_disc_set_artist(disc, unknown.constData());
    cddb_disc_set_title(disc, unknown.constData());
    cddb_disc_set_genre(disc, unknown.constData());
    close(fd);

    emit initialDetails(toAlbum(disc));
}

void Cddb::lookup()
{
    bool isInitial=!disc;
    if (!disc) {
        readDisc();
    }

    if (!disc) {
        // Errors already logged in readDisc
        return;
    }

    cddb_conn_t *connection = cddb_new();
    if (!connection) {
        emit error(i18n("Failed to create CDDB connection"));
        return;
    }

    cddb_set_server_name(connection, Settings::self()->cddbHost().toLatin1().constData());
    cddb_set_server_port(connection, Settings::self()->cddbPort());

    QUrl url;
    url.setHost(Settings::self()->cddbHost());
    url.setPort(Settings::self()->cddbPort());
    QList<QNetworkProxy> proxies=NetworkProxyFactory::self()->queryProxy(QNetworkProxyQuery(url));
    foreach (const QNetworkProxy &p, proxies) {
        if (QNetworkProxy::NoProxy!=p.type()) {
            cddb_set_http_proxy_server_name(connection, p.hostName().toLatin1().constData());
            cddb_set_http_proxy_server_port(connection, p.port());
            cddb_http_proxy_enable(connection);
            break;
        }
    }

    int numMatches = cddb_query(connection, disc);

    if (numMatches<1) {
        if (!isInitial) {
            emit error(i18n("No matches found in CDDB"));
        }
        cddb_destroy(connection);
        return;
    }

    QList<CddbAlbum> m;
    for (int i = 0; i < numMatches; i++) {
        cddb_disc_t *possible = cddb_disc_clone(disc);
        if (!cddb_read(connection, possible)) {
            emit error(i18n("CDDB error: %1").arg(cddb_error_str(cddb_errno(connection))));
            cddb_destroy(connection);
            return;
        }

        int numTracks=cddb_disc_get_track_count(possible);
        if (numTracks<=0) {
            continue;
        }

        CddbAlbum album=toAlbum(possible);
        if (!album.tracks.isEmpty()) {
            m.append(album);
        }

        cddb_disc_destroy(possible);
    }

    cddb_destroy(connection);
    if (m.isEmpty()) {
        if (!isInitial) {
            emit error(i18n("No matches found in CDDB"));
        }
    } else {
        emit matches(m);
    }
}
