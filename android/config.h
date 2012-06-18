#ifndef _CONFIG_H
#define _CONFIG_H

#define PACKAGE_NAME  "cantata"
#define PACKAGE_VERSION "0.8.0"
#define PACKAGE_STRING  PACKAGE_NAME" "PACKAGE_VERSION
#define DEFAULT_ALBUM_ICON "media-optical"
#define DEFAULT_STREAM_ICON "applications-internet"
#define CANTATA_ANDROID
#ifdef ENABLE_WEBKIT
#undef ENABLE_WEBKIT
#endif

#endif

