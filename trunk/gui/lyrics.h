#ifndef LYRICS_H
#define LYRICS_H

#include <QtCore/QObject>
#include <QtCore/QMap>

class QNetworkAccessManager;
class QNetworkReply;
class Song;

class Lyrics : public QObject
{
    Q_OBJECT

public:
    static Lyrics * self();
    Lyrics();

    void get(const Song &song);
    void setMpdDir(const QString &d) { mpdDir=d; }

Q_SIGNALS:
    void lyrics(const QString &artist, const QString &title, const QString &text);

private Q_SLOTS:
    void jobFinished(QNetworkReply *reply);

private:
    struct Job
    {
        Job(QNetworkReply *rep, int r=0) : reply(rep), redirects(r) { }
        QNetworkReply *reply;
        int redirects;
    };

private:
    QNetworkAccessManager *manager;
    QString mpdDir;
    QMap<QString, Job> jobs;
};

#endif
