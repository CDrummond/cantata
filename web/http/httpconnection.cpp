/**
 * @file
 *
 * @author Stefan Frings
 * @author Petr Bravenec petr.bravenec@hobrasoft.cz
 * @author Craig Drummond
 */

#include "httpconnection.h"
#include "httprequest.h"
#include "httprequesthandler.h"
#include "httpresponse.h"
#include "basichttpserver.h"
#include <QTcpSocket>
#include <QSettings>
#include <QDebug>

#define DBUG if (BasicHttpServer::debugEnabled()) qWarning() << metaObject()->className() << (void *)this << __FUNCTION__

static int timeout=-1;

HttpConnection::HttpConnection(BasicHttpServer *parent, QTcpSocket *sock)
    : QObject(parent)
    , socket(sock)
    , response(this)
    , server(parent)
    , handler(0)
{
    DBUG;
    if (-1==::timeout) {
        ::timeout=server->settings()->value("http/timeout", 600).toInt();
    }
    timer = new QTimer(this);
    timer->setInterval(::timeout * 1000);
    timer->setSingleShot(true);
    timer->start();
    connect(timer, SIGNAL(timeout()), this, SLOT(timeout()));
    connect(socket, SIGNAL(readyRead()), this, SLOT(readData()));
    connect(socket, SIGNAL(disconnected()), this, SLOT(disconnected()));
}

HttpConnection::~HttpConnection()
{
    DBUG;
}

void HttpConnection::close()
{
    DBUG;
    socket->disconnectFromHost();
    request.reset();
    socket->deleteLater();
    deleteLater();
}

void HttpConnection::write(const QByteArray &data)
{
    int remaining = data.size();
    const char *ptr = data.data();
    while (socket->isOpen() && remaining>0) {
        int written = socket->write(data);
        socket->waitForBytesWritten(3000);
        ptr += written;
        remaining -= written;
    }
}

void HttpConnection::timeout()
{
    socket->write("HTTP/1.1 408 request timeout\r\n");
    socket->write("Connection: close\r\n");
    socket->write("\r\n");
    socket->write("408 request timeout\r\n");
    socket->disconnectFromHost();
}

void HttpConnection::disconnected()
{
    DBUG;
    socket->close();
    socket->deleteLater();
    timer->stop();
    deleteLater();
}

void HttpConnection::readData()
{
    timer->start();
    while (socket->bytesAvailable() && HttpRequest::StatusComplete!=request.status() && HttpRequest::StatusAbort!=request.status()) {
        request.readFromSocket(socket);
        if (HttpRequest::StatusWaitForBody==request.status()) {
            timer->start();
        }
    }

    if (HttpRequest::StatusAbort==request.status()) {
        socket->write("HTTP/1.1 413 entity too large\r\n");
        socket->write("Connection: close\r\n\r\n");
        socket->write("\r\n");
        socket->write("413 entity too large\r\n");
        socket->disconnectFromHost();
        timer->stop();
        request.reset();
        return;
    }
    
    if (HttpRequest::StatusComplete==request.status()) {
        timer->stop();
        handler=server->getHandler(request.path());
        DBUG << request.path() << (void *)handler;
        if (handler) {
            disconnect(socket, SIGNAL(readyRead()), this, SLOT(readData()));
            response.reinit();
            switch(handler->handle(&request, &response)) {
            case HttpRequestHandler::Status_Handled:
                DBUG << "Handled";
                requestHandled();
                break;
            case HttpRequestHandler::Status_Handling:
                DBUG << "Handling";
                break;
            case HttpRequestHandler::Status_BadRequest:
                DBUG << "Bad request";
                response.setStatus(400, "Bad Request");
                requestHandled();
            }
        } else {
            socket->disconnectFromHost();
        }
    }
}

void HttpConnection::requestHandled()
{
    DBUG << (void *)handler;
    if (response.isClosed()) {
        return;
    }
    response.close();
    if (!response.hasSentLastPart()) {
        response.flush();
    }

    if (request.header("Connection").toLower() == "close" || !socket->bytesAvailable()) {
        DBUG << "disconnectFromHost";
        socket->disconnectFromHost();
    } else {
        DBUG << "restart timer";
        request.reset();
        connect(socket, SIGNAL(readyRead()), this, SLOT(readData()));
        timer->start();
    }

    server->handlerFinished(handler);
}
