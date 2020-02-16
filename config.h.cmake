#ifndef _CONFIG_H
#define _CONFIG_H

#include <QCoreApplication>
#include "support/utils.h"

#if QT_VERSION >= 0x050600
#define DEVICE_PIXEL_RATIO devicePixelRatioF
#else
#define DEVICE_PIXEL_RATIO devicePixelRatio
#endif

#define CANTATA_MAKE_VERSION(a, b, c) (((a) << 16) | ((b) << 8) | (c))
#define PACKAGE_NAME  "@PROJECT_NAME@"
#define ORGANIZATION_NAME "@ORGANIZATION_NAME@"
#define PACKAGE_VERSION CANTATA_MAKE_VERSION(@CPACK_PACKAGE_VERSION_MAJOR@, @CPACK_PACKAGE_VERSION_MINOR@, @CPACK_PACKAGE_VERSION_PATCH@)
#define PACKAGE_STRING  PACKAGE_NAME" @CANTATA_VERSION_FULL@"
#define PACKAGE_VERSION_STRING "@CANTATA_VERSION_WITH_SPIN@"
#define INSTALL_PREFIX "@CMAKE_INSTALL_PREFIX@"
#define SHARE_INSTALL_PREFIX "@SHARE_INSTALL_PREFIX@"
#define ICON_INSTALL_PREFIX "@CANTATA_ICON_INSTALL_PREFIX@"

#cmakedefine ENABLE_DEVICES_SUPPORT 1
#cmakedefine ENABLE_REMOTE_DEVICES 1
#cmakedefine COMPLEX_TAGLIB_FILENAME 1
#cmakedefine TAGLIB_FOUND 1
#cmakedefine TAGLIB_ASF_FOUND
#cmakedefine TAGLIB_MP4_FOUND 1
#cmakedefine TAGLIB_OPUS_FOUND 1
#cmakedefine MTP_FOUND 1
#cmakedefine ENABLE_HTTP_STREAM_PLAYBACK 1
#cmakedefine ENABLE_KWALLET 1
#cmakedefine FFMPEG_FOUND 1
#cmakedefine MPG123_FOUND 1
#cmakedefine CDDB_FOUND 1
#cmakedefine MUSICBRAINZ5_FOUND 1
#cmakedefine ENABLE_REPLAYGAIN_SUPPORT 1
#cmakedefine TAGLIB_CAN_SAVE_ID3VER 1
#cmakedefine ENABLE_PROXY_CONFIG 1
#cmakedefine CDPARANOIA_HAS_CACHEMODEL_SIZE 1
#cmakedefine CDIOPARANOIA_FOUND 1
#cmakedefine QT_QTDBUS_FOUND 1
#cmakedefine ENABLE_HTTP_SERVER 1
#cmakedefine IOKIT_FOUND 1
#cmakedefine QT_MAC_EXTRAS_FOUND 1
#cmakedefine ENABLE_SIMPLE_MPD_SUPPORT 1
#cmakedefine AVAHI_FOUND
#cmakedefine ENABLE_CATEGORIZED_VIEW 1

#cmakedefine HAVE_CDIO_PARANOIA_H 1
#cmakedefine HAVE_CDIO_PARANOIA_PARANOIA_H 1
#cmakedefine HAVE_CDIO_CDDA_H 1
#cmakedefine HAVE_CDIO_PARANOIA_CDDA_H 1

#define CANTATA_REV_URL "@PROJECT_REV_URL@"
#define CANTATA_URL "@PROJECT_URL@"

#define CANTATA_SYS_ICONS_DIR   Utils::systemDir(QLatin1String("icons"))
#define CANTATA_SYS_MPD_DIR     Utils::systemDir(QLatin1String("mpd"))
#define CANTATA_SYS_TRANS_DIR   Utils::systemDir(QLatin1String("translations"))
#define CANTATA_SYS_SCRIPTS_DIR Utils::systemDir(QLatin1String("scripts"))

#define LINUX_LIB_DIR "@LINUX_LIB_DIR@"

#define CANTATA_ICON_THEME "@CANTATA_ICON_THEME@"

#endif
