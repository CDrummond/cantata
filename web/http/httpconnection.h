/**
 * @file
 *
 * @author Stefan Frings
 * @author Petr Bravenec petr.bravenec@hobrasoft.cz
 * @author Craig Drummond
 */

#ifndef HTTP_CONNECTION_H
#define HTTP_CONNECTION_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include "httprequest.h"
#include "httpresponse.h"

class BasicHttpServer;
class HttpRequestHandler;

class HttpConnection : public QObject
{
    Q_OBJECT
public:
    HttpConnection(BasicHttpServer *parent, QTcpSocket *sock);
    ~HttpConnection();

    void close();
    void disconnectFromHost() { socket->disconnectFromHost(); }
    bool isConnected() const { return socket->isOpen(); }

private:
    void write(const QByteArray &data);
    void requestHandled();

private Q_SLOTS:
    void timeout();
    void readData();
    void disconnected();

private:
    QTcpSocket *socket;
    QTimer *timer;
    HttpRequest request;
    HttpResponse response;
    BasicHttpServer *server;
    HttpRequestHandler *handler;

    friend class HttpResponse;    
};

#endif
