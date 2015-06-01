/**
 * @file
 *
 * @author Stefan Frings
 * @author Petr Bravenec petr.bravenec@hobrasoft.cz
 * @author Craig Drummond
 */

#ifndef BASIC_HTTP_SERVER_H
#define BASIC_HTTP_SERVER_H

#include <QObject>
#include <QTcpServer>
#include <QString>

class HttpRequestHandler;
class HttpRequestHandlerFactory;
class HttpConnection;
class StaticFileController;
class QSettings;

class BasicHttpServer : public QTcpServer
{
    Q_OBJECT
public:
    static void enableDebug() { dbgEnabled=true; }
    static bool debugEnabled() { return dbgEnabled; }

    BasicHttpServer(QObject *parent, const QSettings *s);

    bool start();
    const QSettings * settings() const { return serverSettings; }

    virtual HttpRequestHandler * getHandler(const QString &path);
    virtual void handlerFinished(HttpRequestHandler *handler) { Q_UNUSED(handler) }

private Q_SLOTS:
    void  slotNewConnection();

protected:
    const QSettings *serverSettings;
    StaticFileController *staticFiles;

private:
    static bool dbgEnabled;
};

#endif
