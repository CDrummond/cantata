#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KConfigGroup>
namespace KWallet {
class Wallet;
}
#else
#include <QtCore/QSettings>
#endif

class Settings
{
public:
    static Settings *self();

    Settings();
    ~Settings();

    QString connectionHost();
    QString connectionPasswd();
    int connectionPort();
    bool consumePlaylist();
    bool randomPlaylist();
    bool repeatPlaylist();
    bool showPlaylist();
    QByteArray playlistHeaderState();
    QByteArray splitterState();
#ifndef ENABLE_KDE_SUPPORT
    QSize mainWindowSize();
#endif
    bool useSystemTray();
    bool showPopups();
    QString mpdDir();
    int coverSize();
    int sidebar();
    QStringList lyricProviders();

    void saveConnectionHost(const QString &v);
    void saveConnectionPasswd(const QString &v);
    void saveConnectionPort(int v);
    void saveConsumePlaylist(bool v);
    void saveRandomPlaylist(bool v);
    void saveRepeatPlaylist(bool v);
    void saveShowPlaylist(bool v);
    void savePlaylistHeaderState(const QByteArray &v);
    void saveSplitterState(const QByteArray &v);
#ifndef ENABLE_KDE_SUPPORT
    void saveMainWindowSize(const QSize &v);
#endif
    void saveUseSystemTray(bool v);
    void saveShowPopups(bool v);
    void saveMpdDir(const QString &v);
    void saveCoverSize(int v);
    void saveSidebar(int v);
    void saveLyricProviders(const QStringList &p);
    void save();
#ifdef ENABLE_KDE_SUPPORT
    bool openWallet();
#endif

private:
#ifdef ENABLE_KDE_SUPPORT
    KConfigGroup cfg;
    KWallet::Wallet *wallet;
    QString passwd;
#else
    QSettings cfg;
#endif
};

#endif
