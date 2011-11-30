#ifndef LYRICSPAGE_H_
#define LYRICSPAGE_H_

#include <QWidget>
// #include <QScopedPointer>
#include "song.h"
#include "ui_lyricspage.h"

class UltimateLyricsProvider;
// class UltimateLyricsReader;
class UltimateLyricsProvider;

class LyricsPage : public QWidget, public Ui::LyricsPage
{
  Q_OBJECT

public:
    LyricsPage(QWidget *parent);
    ~LyricsPage();

    void setEnabledProviders(const QStringList &providerList);
    void update(const Song &song, bool force=false);
    const QList<UltimateLyricsProvider *> & getProviders() { return providers; }
    void setMpdDir(const QString &d) { mpdDir=d; }

Q_SIGNALS:
    void providersUpdated();

protected Q_SLOTS:
    void resultReady(int id, const QString &lyrics);
    void update();

private:
    UltimateLyricsProvider * providerByName(const QString &name) const;
    void getLyrics();

// private Q_SLOTS:
//     void ultimateLyricsParsed();

private:
    QString mpdDir;
//     QScopedPointer<UltimateLyricsReader> reader;
    QList<UltimateLyricsProvider *> providers;
    int currentProvider;
    int currentRequest;
    Song currentSong;
    QAction *refreshAction;
};

#endif
