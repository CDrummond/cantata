/*
    Copyright 2010 Michael Zanetti <mzanetti@kde.org>
    Copyright 2010, 2011 Lukas Tinkl <ltinkl@redhat.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) version 3, or any
    later version accepted by the membership of KDE e.V. (or its
    successor approved by the membership of KDE e.V.), which shall
    act as a proxy defined in Section 6 of version 3 of the license.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library. If not, see <http://www.gnu.org/licenses/>.
*/

#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <QtCore/QFile>
#include <QtCore/QMap>
#include <QtCore/QMutexLocker>

#include "udisksopticaldisc.h"
#include "soliddefs_p.h"

typedef QMap<QString, Solid::OpticalDisc::ContentTypes> ContentTypesCache;
SOLID_GLOBAL_STATIC(ContentTypesCache, cache)
SOLID_GLOBAL_STATIC(QMutex, cacheLock)

// inspired by http://cgit.freedesktop.org/hal/tree/hald/linux/probing/probe-volume.c
static Solid::OpticalDisc::ContentType advancedDiscDetect(const QString & device_file)
{
    /* the discs block size */
    unsigned short bs;
    /* the path table size */
    unsigned short ts;
    /* the path table location (in blocks) */
    unsigned int tl;
    /* length of the directory name in current path table entry */
    unsigned char len_di = 0;
    /* the number of the parent directory's path table entry */
    unsigned int parent = 0;
    /* filename for the current path table entry */
    char dirname[256];
    /* our position into the path table */
    int pos = 0;
    /* the path table record we're on */
    int curr_record = 1;

    Solid::OpticalDisc::ContentType result = Solid::OpticalDisc::NoContent;

    int fd = open (QFile::encodeName(device_file), O_RDONLY);

    /* read the block size */
    lseek (fd, 0x8080, SEEK_CUR);
    if (read (fd, &bs, 2) != 2)
    {
        qDebug("Advanced probing on %s failed while reading block size", qPrintable(device_file));
        goto out;
    }

    /* read in size of path table */
    lseek (fd, 2, SEEK_CUR);
    if (read (fd, &ts, 2) != 2)
    {
        qDebug("Advanced probing on %s failed while reading path table size", qPrintable(device_file));
        goto out;
    }

    /* read in which block path table is in */
    lseek (fd, 6, SEEK_CUR);
    if (read (fd, &tl, 4) != 4)
    {
        qDebug("Advanced probing on %s failed while reading path table block", qPrintable(device_file));
        goto out;
    }

    /* seek to the path table */
    lseek (fd, bs * tl, SEEK_SET);

    /* loop through the path table entries */
    while (pos < ts)
    {
        /* get the length of the filename of the current entry */
        if (read (fd, &len_di, 1) != 1)
        {
            qDebug("Advanced probing on %s failed, cannot read more entries", qPrintable(device_file));
            break;
        }

        /* get the record number of this entry's parent
           i'm pretty sure that the 1st entry is always the top directory */
        lseek (fd, 5, SEEK_CUR);
        if (read (fd, &parent, 2) != 2)
        {
            qDebug("Advanced probing on %s failed, couldn't read parent entry", qPrintable(device_file));
            break;
        }

        /* read the name */
        if (read (fd, dirname, len_di) != len_di)
        {
            qDebug("Advanced probing on %s failed, couldn't read the entry name", qPrintable(device_file));
            break;
        }
        dirname[len_di] = 0;

        /* if we found a folder that has the root as a parent, and the directory name matches
           one of the special directories then set the properties accordingly */
        if (parent == 1)
        {
            if (!strcasecmp (dirname, "VIDEO_TS"))
            {
                qDebug("Disc in %s is a Video DVD", qPrintable(device_file));
                result = Solid::OpticalDisc::VideoDvd;
                break;
            }
            else if (!strcasecmp (dirname, "BDMV"))
            {
                qDebug("Disc in %s is a Blu-ray video disc", qPrintable(device_file));
                result = Solid::OpticalDisc::VideoBluRay;
                break;
            }
            else if (!strcasecmp (dirname, "VCD"))
            {
                qDebug("Disc in %s is a Video CD", qPrintable(device_file));
                result = Solid::OpticalDisc::VideoCd;
                break;
            }
            else if (!strcasecmp (dirname, "SVCD"))
            {
                qDebug("Disc in %s is a Super Video CD", qPrintable(device_file));
                result = Solid::OpticalDisc::SuperVideoCd;
                break;
            }
        }

        /* all path table entries are padded to be even,
           so if this is an odd-length table, seek a byte to fix it */
        if (len_di%2 == 1)
        {
            lseek (fd, 1, SEEK_CUR);
            pos++;
        }

        /* update our position */
        pos += 8 + len_di;
        curr_record++;
    }

    close(fd);
    return result;

out:
    /* go back to the start of the file */
    lseek (fd, 0, SEEK_SET);
    close(fd);
    return result;
}

using namespace Solid::Backends::UDisks;

OpticalDisc::OpticalDisc(UDisksDevice *device)
    : UDisksStorageVolume(device), m_needsReprobe(true), m_cachedContent(Solid::OpticalDisc::NoContent)
{
    connect(device, SIGNAL(changed()), this, SLOT(slotChanged()));
}

OpticalDisc::~OpticalDisc()
{

}

qulonglong OpticalDisc::capacity() const
{
    return m_device->prop("DeviceSize").toULongLong();
}

bool OpticalDisc::isRewritable() const
{
    // the hard way, udisks has no notion of a disc "rewritability"
    const QString mediaType = m_device->prop("DriveMedia").toString();
    return mediaType == "optical_cd_rw" || mediaType == "optical_dvd_rw" || mediaType == "optical_dvd_ram" ||
            mediaType == "optical_dvd_plus_rw" || mediaType == "optical_dvd_plus_rw_dl" ||
            mediaType == "optical_bd_re" || mediaType == "optical_hddvd_rw"; // TODO check completeness
}

bool OpticalDisc::isBlank() const
{
    return m_device->prop("OpticalDiscIsBlank").toBool();
}

bool OpticalDisc::isAppendable() const
{
    return m_device->prop("OpticalDiscIsAppendable").toBool();
}

Solid::OpticalDisc::DiscType OpticalDisc::discType() const
{
    const QString discType = m_device->prop("DriveMedia").toString();

    QMap<Solid::OpticalDisc::DiscType, QString> map;
    map[Solid::OpticalDisc::CdRom] = "optical_cd";
    map[Solid::OpticalDisc::CdRecordable] = "optical_cd_r";
    map[Solid::OpticalDisc::CdRewritable] = "optical_cd_rw";
    map[Solid::OpticalDisc::DvdRom] = "optical_dvd";
    map[Solid::OpticalDisc::DvdRecordable] = "optical_dvd_r";
    map[Solid::OpticalDisc::DvdRewritable] ="optical_dvd_rw";
    map[Solid::OpticalDisc::DvdRam] ="optical_dvd_ram";
    map[Solid::OpticalDisc::DvdPlusRecordable] ="optical_dvd_plus_r";
    map[Solid::OpticalDisc::DvdPlusRewritable] ="optical_dvd_plus_rw";
    map[Solid::OpticalDisc::DvdPlusRecordableDuallayer] ="optical_dvd_plus_r_dl";
    map[Solid::OpticalDisc::DvdPlusRewritableDuallayer] ="optical_dvd_plus_rw_dl";
    map[Solid::OpticalDisc::BluRayRom] ="optical_bd";
    map[Solid::OpticalDisc::BluRayRecordable] ="optical_bd_r";
    map[Solid::OpticalDisc::BluRayRewritable] ="optical_bd_re";
    map[Solid::OpticalDisc::HdDvdRom] ="optical_hddvd";
    map[Solid::OpticalDisc::HdDvdRecordable] ="optical_hddvd_r";
    map[Solid::OpticalDisc::HdDvdRewritable] ="optical_hddvd_rw";
    // TODO add these to Solid
    //map[Solid::OpticalDisc::MagnetoOptical] ="optical_mo";
    //map[Solid::OpticalDisc::MountRainer] ="optical_mrw";
    //map[Solid::OpticalDisc::MountRainerWritable] ="optical_mrw_w";

    return map.key( discType, Solid::OpticalDisc::UnknownDiscType );
}

Solid::OpticalDisc::ContentTypes OpticalDisc::availableContent() const
{
    if (isBlank()) {
        m_needsReprobe = false;
        return Solid::OpticalDisc::NoContent;
    }

    if (m_needsReprobe) {
        QMutexLocker lock(cacheLock);

        QString deviceFile = m_device->prop("DeviceFile").toString();

        if (cache->contains(deviceFile)) {
            m_cachedContent = cache->value(deviceFile);
            m_needsReprobe = false;
            return m_cachedContent;
        }

        m_cachedContent = Solid::OpticalDisc::NoContent;
        bool hasData = m_device->prop("OpticalDiscNumTracks").toInt() > 0 &&
                        m_device->prop("OpticalDiscNumTracks").toInt() > m_device->prop("OpticalDiscNumAudioTracks").toInt();
        bool hasAudio = m_device->prop("OpticalDiscNumAudioTracks").toInt() > 0;

        if ( hasData ) {
            m_cachedContent |= Solid::OpticalDisc::Data;
            m_cachedContent |= advancedDiscDetect(deviceFile);
        }
        if ( hasAudio )
            m_cachedContent |= Solid::OpticalDisc::Audio;

        m_needsReprobe = false;

        cache->insert(deviceFile, m_cachedContent);
    }

    return m_cachedContent;
}

void OpticalDisc::slotChanged()
{
    QMutexLocker lock(cacheLock);
    m_needsReprobe = true;
    m_cachedContent = Solid::OpticalDisc::NoContent;
    cache->remove(m_device->prop("DeviceFile").toString());
}
