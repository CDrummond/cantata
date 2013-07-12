#ifndef _CONFIG_H
#define _CONFIG_H

#define CANTATA_MAKE_VERSION(a, b, c) (((a) << 16) | ((b) << 8) | (c))
/*
  NOTE: If CANTATA_REV_URL is changed, then cantata-dynamac, CMakeLists.txt, and README will need updating.
        dbus/com.googlecode.cantata.xml will also need renaming/updating.
*/
#define CANTATA_REV_URL "com.googlecode.cantata"
#define CANTATA_URL "cantata.googlecode.com"

#define PACKAGE_NAME  "@PROJECT_NAME@"
#define PACKAGE_VERSION CANTATA_MAKE_VERSION(@CPACK_PACKAGE_VERSION_MAJOR@, @CPACK_PACKAGE_VERSION_MINOR@, @CPACK_PACKAGE_VERSION_PATCH@)
#define PACKAGE_STRING  PACKAGE_NAME" @CANTATA_VERSION_FULL@"
#define PACKAGE_VERSION_STRING "@CANTATA_VERSION_FULL@"
#define INSTALL_PREFIX "@CMAKE_INSTALL_PREFIX@"

#cmakedefine ENABLE_DEVICES_SUPPORT 1
#cmakedefine COMPLEX_TAGLIB_FILENAME 1
#cmakedefine TAGLIB_FOUND 1
#cmakedefine TAGLIB_EXTRAS_FOUND 1
#cmakedefine TAGLIB_ASF_FOUND
#cmakedefine TAGLIB_MP4_FOUND 1
#cmakedefine MTP_FOUND 1
#cmakedefine ENABLE_HTTP_STREAM_PLAYBACK 1
#cmakedefine FFMPEG_FOUND 1
#cmakedefine MPG123_FOUND 1
#cmakedefine CDDB_FOUND 1
#cmakedefine MUSICBRAINZ5_FOUND 1
#cmakedefine USE_SPEEX_RESAMPLER 1
#cmakedefine ENABLE_REPLAYGAIN_SUPPORT 1
#cmakedefine ENABLE_REMOTE_DEVICES 1
#cmakedefine TAGLIB_CAN_SAVE_ID3VER 1
#cmakedefine ENABLE_PROXY_CONFIG 1

/*
 This is done via CMake add_defintions - as it controls SLOT generation in GtkProxyStyle
 - hence adding _XXX
*/
#cmakedefine ENABLE_OVERLAYSCROLLBARS_XXX 1
#endif

