#include "settings.h"
#include "config.h"
#include "musiclibraryitemalbum.h"
#include "fancytabwidget.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobal>
#include <KDE/KConfig>
#include "kwallet.h"
#include <QtGui/QApplication>
#include <QtGui/QWidget>

K_GLOBAL_STATIC(Settings, instance)
#endif

Settings * Settings::self()
{
#ifdef ENABLE_KDE_SUPPORT
    return instance;
#else
    static Settings *instance=0;;
    if(!instance) {
        instance=new Settings;
    }
    return instance;
#endif
}

Settings::Settings()
#ifdef ENABLE_KDE_SUPPORT
        : cfg(KGlobal::config(), "General")
        , wallet(0)
#endif
{
}

Settings::~Settings()
{
#ifdef ENABLE_KDE_SUPPORT
    delete wallet;
#endif
}

#ifdef ENABLE_KDE_SUPPORT
#define GET_STRING(KEY, DEF) (cfg.readEntry(KEY, QString(DEF)))
#define GET_BOOL(KEY, DEF)   (cfg.readEntry(KEY, DEF))
#define GET_INT(KEY, DEF)    (cfg.readEntry(KEY, DEF))
#define GET_BYTE_ARRAY(KEY)  (cfg.readEntry(KEY, QByteArray()))
#define GET_SIZE(KEY)        (cfg.readEntry(KEY, QSize()))
#define SET_VALUE(KEY, V)    (cfg.writeEntry(KEY, V))
#else
#define GET_STRING(KEY, DEF) (cfg.contains(KEY) ? cfg.value(KEY).toString() : QString(DEF))
#define GET_BOOL(KEY, DEF)   (cfg.contains(KEY) ? cfg.value(KEY).toBool() : DEF)
#define GET_INT(KEY, DEF)    (cfg.contains(KEY) ? cfg.value(KEY).toInt() : DEF)
#define GET_BYTE_ARRAY(KEY)  (cfg.value(KEY).toByteArray())
#define GET_SIZE(KEY)        (cfg.contains(KEY) ? cfg.value(KEY).toSize() : QSize())
#define SET_VALUE(KEY, V)    (cfg.setValue(KEY, V))
#endif

QString Settings::connectionHost()
{
    return GET_STRING("connectionHost", "localhost");
}

bool Settings::openWallet()
{
    if(wallet) {
        return true;
    }

    wallet=KWallet::Wallet::openWallet(KWallet::Wallet::LocalWallet(), QApplication::activeWindow() ? QApplication::activeWindow()->winId() : 0);
    if(wallet) {
        if(!wallet->hasFolder(PACKAGE_NAME)) {
            wallet->createFolder(PACKAGE_NAME);
        }
        wallet->setFolder(PACKAGE_NAME);
        return true;
    }

    return false;
}

QString Settings::connectionPasswd()
{
#ifdef ENABLE_KDE_SUPPORT
    if(passwd.isEmpty() && GET_BOOL("connectionPasswd", false) && openWallet()) {
        wallet->readPassword("mpd", passwd);
    }
    return passwd;
#else
    return GET_STRING("connectionPasswd", "");
#endif
}

int Settings::connectionPort()
{
    return GET_INT("connectionPort", 6600);
}

bool Settings::consumePlaylist()
{
    return GET_BOOL("consumePlaylist", false);
}

bool Settings::randomPlaylist()
{
    return GET_BOOL("randomPlaylist", false);
}

bool Settings::repeatPlaylist()
{
    return GET_BOOL("repeatPlaylist", true);
}

bool Settings::showPlaylist()
{
    return GET_BOOL("showPlaylist", true);
}

QByteArray Settings::playlistHeaderState()
{
    return GET_BYTE_ARRAY("playlistHeaderState");
}

QByteArray Settings::splitterState()
{
    return GET_BYTE_ARRAY("splitterState");
}

#ifndef ENABLE_KDE_SUPPORT
QSize Settings::mainWindowSize()
{
    return GET_SIZE("mainWindowSize");
}
#endif

bool Settings::useSystemTray()
{
    return GET_BOOL("useSystemTray", false);
}

bool Settings::showPopups()
{
    return GET_BOOL("showPopups", false);
}

QString Settings::mpdDir()
{
    QString d=GET_STRING("mpdDir", "/var/lib/mpd/music");
    if (!d.endsWith("/")) {
        d=d+"/";
    }
    return d;
}

int Settings::coverSize()
{
    return GET_INT("coverSize", (int)(MusicLibraryItemAlbum::CoverMedium));
}

int Settings::sidebar()
{
    return GET_INT("sidebar", (int)(FancyTabWidget::Mode_LargeSidebar));
}

void Settings::saveConnectionHost(const QString &v)
{
    SET_VALUE("connectionHost", v);
}

void Settings::saveConnectionPasswd(const QString &v)
{
#ifdef ENABLE_KDE_SUPPORT
    if(v!=passwd) {
        passwd=v;
        SET_VALUE("connectionPasswd", !passwd.isEmpty());

        if(passwd.isEmpty()) {
            if(wallet) {
                wallet->removeEntry("mpd");
            }
        }
        else if(openWallet()) {
            wallet->writePassword("mpd", passwd);
        }
    }
#else
    SET_VALUE("connectionPasswd", v);
#endif
}

void Settings::saveConnectionPort(int v)
{
    SET_VALUE("connectionPort", v);
}

void Settings::saveConsumePlaylist(bool v)
{
    SET_VALUE("consumePlaylist", v);
}

void Settings::saveRandomPlaylist(bool v)
{
    SET_VALUE("randomPlaylist", v);
}

void Settings::saveRepeatPlaylist(bool v)
{
    SET_VALUE("repeatPlaylist", v);
}

void Settings::saveShowPlaylist(bool v)
{
    SET_VALUE("showPlaylist", v);
}

void Settings::savePlaylistHeaderState(const QByteArray &v)
{
    SET_VALUE("playlistHeaderState", v);
}

void Settings::saveSplitterState(const QByteArray &v)
{
    SET_VALUE("splitterState", v);
}

#ifndef ENABLE_KDE_SUPPORT
void Settings::saveMainWindowSize(const QSize &v)
{
    SET_VALUE("mainWindowSize", v);
}
#endif

void Settings::saveUseSystemTray(bool v)
{
    SET_VALUE("useSystemTray", v);
}

void Settings::saveShowPopups(bool v)
{
    SET_VALUE("showPopups", v);
}

void Settings::saveMpdDir(const QString &v)
{
    QString d(v);
    if (!d.isEmpty() && !d.endsWith("/")) {
        d=d+"/";
    }
    SET_VALUE("mpdDir", d);
}

void Settings::saveCoverSize(int v)
{
    SET_VALUE("coverSize", v);
}

void Settings::saveSidebar(int v)
{
    SET_VALUE("sidebar", v);
}

void Settings::save()
{
#ifdef ENABLE_KDE_SUPPORT
    KGlobal::config()->sync();
#endif
}
