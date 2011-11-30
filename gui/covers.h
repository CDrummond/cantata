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
    QMap<QString, QNetworkReply*>::Iterator findJob(QNetworkReply *reply);

private:
    QString mpdDir;
    MaiaXmlRpcClient *rpc;
    NetworkAccessManager *manager;
    QMap<QString, QNetworkReply*> jobs;
};

#endif