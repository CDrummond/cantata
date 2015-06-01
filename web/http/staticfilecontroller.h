/**
 * @file
 *
 * @author Stefan Frings
 * @author Petr Bravenec petr.bravenec@hobrasoft.cz
 * @author Craig Drummond
 */

#ifndef STATIC_FILE_CONTROLLER_H
#define STATIC_FILE_CONTROLLER_H

//#include <QCache>
#include "httprequesthandler.h"

class HttpRequest;
class HttpResponse;
class HttpServer;
class QSettings;

class StaticFileController : public HttpRequestHandler
{
public:
    StaticFileController(const QSettings *settings);

    HandleStatus handle(HttpRequest *request, HttpResponse *response);
    static void addMimeType(const QByteArray &extension, const QByteArray &mimetype);
    static QByteArray mimeType(const QByteArray &extension);
    static void setContentType(const QString &ext, HttpResponse *response);

//private:
//    struct CacheEntry {
//        QByteArray document;
//        qint64 created;
//    };

//    QCache<QString, CacheEntry> cache;
};

#endif
