#ifndef _CONFIG_H
#define _CONFIG_H

#include <QCoreApplication>
#include "support/utils.h"

#define CANTATA_MAKE_VERSION(a, b, c) (((a) << 16) | ((b) << 8) | (c))
#define PACKAGE_NAME  "@PROJECT_NAME@"
#define ORGANIZATION_NAME "@ORGANIZATION_NAME@"
#define PACKAGE_VERSION CANTATA_MAKE_VERSION(@CPACK_PACKAGE_VERSION_MAJOR@, @CPACK_PACKAGE_VERSION_MINOR@, @CPACK_PACKAGE_VERSION_PATCH@)
#define PACKAGE_STRING  PACKAGE_NAME" @CANTATA_VERSION_FULL@"
#define PACKAGE_VERSION_STRING "@CANTATA_VERSION_WITH_SPIN@"
#define INSTALL_PREFIX "@CMAKE_INSTALL_PREFIX@"

#cmakedefine ENABLE_DEVICES_SUPPORT 1
#cmakedefine ENABLE_REMOTE_DEVICES 1
#cmakedefine ENABLE_ONLINE_SERVICES 1
#cmakedefine ENABLE_STREAMS 1
#cmakedefine ENABLE_DYNAMIC 1
#cmakedefine COMPLEX_TAGLIB_FILENAME 1
#cmakedefine TAGLIB_FOUND 1
#cmakedefine TAGLIB_EXTRAS_FOUND 1
#cmakedefine TAGLIB_ASF_FOUND
#cmakedefine TAGLIB_MP4_FOUND 1
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
#cmakedefine ENABLE_EXTERNAL_TAGS 1
#cmakedefine CDPARANOIA_HAS_CACHEMODEL_SIZE 1
#cmakedefine QT_QTDBUS_FOUND 1
#cmakedefine ENABLE_UNCACHED_MTP 1
#cmakedefine ENABLE_HTTP_SERVER 1
#cmakedefine ENABLE_MODEL_TEST 1
#cmakedefine USE_SYSTEM_MENU_ICON 1
#cmakedefine ENABLE_UBUNTU 1
#cmakedefine IOKIT_FOUND 1
#cmakedefine QT_MAC_EXTRAS_FOUND 1
#cmakedefine UNITY_MENU_HACK 1

#ifdef ENABLE_UBUNTU
#define CANTATA_REV_URL "com.ubuntu.developer.nikwen.cantata-touch-reboot" //Sadly, it requires the com.ubuntu.developer.nikwen prefix to be published to the click store
#else
#define CANTATA_REV_URL "@PROJECT_REV_URL@"
#endif
#define CANTATA_URL "@PROJECT_URL@"

#define CANTATA_SYS_CONFIG_DIR  Utils::systemDir(QLatin1String("config"))
#define CANTATA_SYS_ICONS_DIR   Utils::systemDir(QLatin1String("icons"))
#define CANTATA_SYS_MPD_DIR     Utils::systemDir(QLatin1String("mpd"))
#define CANTATA_SYS_TRANS_DIR   Utils::systemDir(QLatin1String("translations"))
#define CANTATA_SYS_SCRIPTS_DIR Utils::systemDir(QLatin1String("scripts"))

#ifdef UNITY_MENU_HACK
#define HIDE_MENU_ICON(A) menuIcons ? A : Icon()
#define HIDE_MENU_ICON_NAME(A) menuIcons ? A : 0
#define UNITY_MENU_ICON_CHECK bool menuIcons=Utils::Unity!=Utils::currentDe() && !QCoreApplication::testAttribute(Qt::AA_DontShowIconsInMenus);
#else
#define HIDE_MENU_ICON(A) A
#define HIDE_MENU_ICON_NAME(A) A
#define UNITY_MENU_ICON_CHECK
#endif

#define LINUX_LIB_DIR "@LINUX_LIB_DIR@"

#endif
