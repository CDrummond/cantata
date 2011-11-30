#include "networkaccessmanager.h"
#include <QtCore/QTimerEvent>

NetworkAccessManager::NetworkAccessManager(QObject *parent)
    : BASE_NETWORK_ACCESS_MANAGER(parent)
{
}

QNetworkReply * NetworkAccessManager::get(const QNetworkRequest &req, int timeout)
{
    QNetworkReply *reply=BASE_NETWORK_ACCESS_MANAGER::get(req);

    if (0!=timeout) {
        connect(reply, SIGNAL(destroyed()), SLOT(replyFinished()));
        connect(reply, SIGNAL(finished()), SLOT(replyFinished()));
        timers[reply] = startTimer(timeout);
    }
    return reply;
}

void NetworkAccessManager::replyFinished()
{
    QNetworkReply *reply = reinterpret_cast<QNetworkReply*>(sender());
    if (timers.contains(reply)) {
        killTimer(timers.take(reply));
    }
}

void NetworkAccessManager::timerEvent(QTimerEvent *e)
{
    QNetworkReply *reply = timers.key(e->timerId());
    if (reply) {
        reply->abort();
    }
}
