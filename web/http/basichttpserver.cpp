/**
 * @file
 *
 * @author Petr Bravenec petr.bravenec@hobrasoft.cz
 * @author Craig Drummond
 */

#include "basichttpserver.h"
#include "httpconnection.h"
#include "httprequesthandler.h"
#include "staticfilecontroller.h"
#include <QSettings>
#include <QDebug>

bool BasicHttpServer::dbgEnabled=false;

#define DBUG if (BasicHttpServer::debugEnabled()) qWarning() << metaObject()->className() << (void *)this << __FUNCTION__

BasicHttpServer::BasicHttpServer(QObject *parent, const QSettings *s)
    : QTcpServer(parent)
    , serverSettings(s)
    , staticFiles(0)
{
}

bool BasicHttpServer::start()
{
    close();
    connect(this, SIGNAL(newConnection()), this, SLOT(slotNewConnection()));
    listen(QHostAddress::Any, serverSettings->value("http/port", 80).toInt());
    return isListening();
}

void BasicHttpServer::slotNewConnection()
{
    while (hasPendingConnections()) {
        DBUG;
        new HttpConnection(this, nextPendingConnection());
    }
}

HttpRequestHandler * BasicHttpServer::getHandler(const QString &path)
{
    Q_UNUSED(path)
    if (!staticFiles) {
        staticFiles=new StaticFileController(serverSettings);
    }
    return staticFiles;
}
