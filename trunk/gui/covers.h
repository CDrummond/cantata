#ifndef COVERS_H
#define COVERS_H

#include <QtCore/QObject>
#include <QtCore/QMap>

class Song;
class QImage;
class QString;
class NetworkAccessManager;
class QNetworkReply;
class MaiaXmlRpcClient;

class Covers : public QObject
{
    Q_OBJECT

public:
    struct Job
    {
        Job(const QString &ar, const QString &al, const QString &d)
            : artist(ar), album(al), dir(d) { }
        QString artist;
        QString album;
        QString dir;
    };

    static Covers * self();
    Covers();

    void get(const Song &song);
    void setMpdDir(const QString &dir) { mpdDir=dir; }

Q_SIGNALS:
    void cover(const QString &artist, const QString &album, const QImage &img);

private Q_SLOTS:
    void albumInfo(QVariant &value, QNetworkReply *reply);
    void albumFailure(int, const QString &, QNetworkReply *reply);
    void jobFinished(QNetworkReply *reply);

private:
    void saveImg(const Job &job, const QImage &img, const QByteArray &raw);
    QMap<QNetworkReply *, Job>::Iterator findJob(const Song &song);

private:
    QString mpdDir;
    MaiaXmlRpcClient *rpc;
    NetworkAccessManager *manager;
    QMap<QNetworkReply *, Job> jobs;
};

#endif