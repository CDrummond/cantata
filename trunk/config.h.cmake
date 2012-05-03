#ifndef _CONFIG_H
#define _CONFIG_H

#define PACKAGE_NAME  "@PROJECT_NAME@"
#define PACKAGE_VERSION "@CANTATA_VERSION_FULL@"
#define PACKAGE_STRING  PACKAGE_NAME" "PACKAGE_VERSION
#define DEFAULT_ALBUM_ICON "media-optical"
#define DEFAULT_STREAM_ICON "applications-internet"

#cmakedefine ENABLE_DEVICES_SUPPORT 1
#cmakedefine COMPLEX_TAGLIB_FILENAME 1
#cmakedefine TAGLIB_EXTRAS_FOUND 1
#cmakedefine TAGLIB_ASF_FOUND
#cmakedefine TAGLIB_MP4_FOUND 1
#cmakedefine MTP_FOUND 1
#cmakedefine CANTATA_DBUG_TIMER
#cmakedefine FFMPEG_FOUND 1
#cmakedefine MPG123_FOUND 1
#cmakedefine USE_SPEEX_RESAMPLER 1
#cmakedefine ENABLE_REPLAYGAIN_SUPPORT 1
#cmakedefine ENABLE_REMOTE_DEVICES 1
#cmakedefine ENABLE_KIO_REMOTE_DEVICES 1

#endif

