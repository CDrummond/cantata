#ifndef NETWORK_ACCESS_MANAGER_H
#define NETWORK_ACCESS_MANAGER_H

#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KIO/AccessManager>
#define BASE_NETWORK_ACCESS_MANAGER KIO::Integration::AccessManager
#else
#include <QtNetwork/QNetworkAccessManager>
#define BASE_NETWORK_ACCESS_MANAGER QNetworkAccessManager
#endif
#include <QtNetwork/QNetworkReply>
#include <QtCore/QMap>

class QTimerEvent;

class NetworkAccessManager : public BASE_NETWORK_ACCESS_MANAGER
{
    Q_OBJECT

public:
    NetworkAccessManager(QObject *parent);
    virtual ~NetworkAccessManager() { }

    QNetworkReply * get(const QNetworkRequest &req, int timeout=0);
    QNetworkReply * get(const QUrl &url, int timeout=0) { return get(QNetworkRequest(url), timeout); }

protected:
    void timerEvent(QTimerEvent *e);

private Q_SLOTS:
    void replyFinished();

private:
    QMap<QNetworkReply *, int> timers;
};

#endif // NETWORK_ACCESS_MANAGER_H
