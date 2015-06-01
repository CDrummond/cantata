/**
 * @file
 *
 * @author Stefan Frings
 * @author Petr Bravenec petr.bravenec@hobrasoft.cz
 * @author Craig Drummond
 */

#include "staticfilecontroller.h"
#include "httpconnection.h"
#include "httpresponse.h"
#include "httprequest.h"
#include "basichttpserver.h"
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QDebug>
#include <QRegExp>
#include <QSettings>
#include <QDebug>

#define DBUG if (BasicHttpServer::debugEnabled()) qWarning() << "StaticFileController" << (void *)this << __FUNCTION__

static QString docroot;
static QString indexFile;
static bool cacheOn=false;
static int cacheTimeout=-1;
static int maxAge=0;
static int maxCachedFileSize=0;
static QHash<QByteArray, QByteArray> mimetypes;

static void initMimeTypes()
{
    if (mimetypes.isEmpty()) {
        mimetypes.insert("png",   "image/png");
        mimetypes.insert("jpeg",  "image/jpeg");
        mimetypes.insert("jpg",   "image/jpeg");
        mimetypes.insert("gif",   "image/gif");
        mimetypes.insert("txt",   "text/plain");
        mimetypes.insert("html",  "text/html");
        mimetypes.insert("xhtml", "text/html");
        mimetypes.insert("htm",   "text/html");
        mimetypes.insert("css",   "text/css");
        mimetypes.insert("json",  "application/json");
        mimetypes.insert("js",    "application/javascript");
    }
}

StaticFileController::StaticFileController(const QSettings *settings)
{
    if (-1==cacheTimeout) {
        docroot=settings->value("http/root", "/var/www").toString();
        indexFile=settings->value("http/index", "index.html").toString();
//        cacheOn=settings->value("http/cacheOn", false).toBool();
//        cacheTimeout=settings->value("http/cacheTimeout", 600000).toInt();
//        maxAge=settings->value("http/maxAge", 3600).toInt();
//        maxCachedFileSize=settings->value("http/maxCachedFileSize", 1000000).toInt();
    }
    initMimeTypes();
}

void StaticFileController::addMimeType(const QByteArray &extension, const QByteArray &mimetype)
{
    initMimeTypes();
    mimetypes.insert(extension, mimetype);
}

QByteArray StaticFileController::mimeType(const QByteArray &extension)
{
    return mimetypes.contains(extension) ? mimetypes[extension] : QByteArray();
}

HttpRequestHandler::HandleStatus StaticFileController::handle(HttpRequest *request, HttpResponse *response)
{
    QString path = request->path();

    if (path.startsWith("/..")) {
        response->setStatus(403,"Forbidden");
        response->write("403 Forbidden");
        response->flush();
        return Status_Handled;
    }

    if (QFileInfo(docroot+path).isDir()) {
        path += "/" + indexFile;
    }

//    qint64 now = QDateTime::currentDateTime().toTime_t() * 1000;
//    CacheEntry *entry = cache.object(path);
//    // Found in cache
//    if (cacheOn && entry && (cacheTimeout == 0 || entry->created > now-cacheTimeout)) {
//        setContentType(path, response);
//        response->setHeader("Cache-Control", "max-age=" + QByteArray::number((int)(maxAge/1000)));
//        response->write(entry->document);
//        return Status_Handled;
//    }

    QFile file(docroot+path);
    if (!file.exists()) {
        DBUG << "404 Not found: " << docroot+path;
        response->setStatus(404, "Not found");
        response->write("404 Not found");
        response->flush();
        return Status_Handled;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        DBUG << "403 Forbidden: " << docroot+path;
        response->setStatus(403, "Forbidden");
        response->write("403 Forbidden");
        response->flush();
        return Status_Handled;
    }

    setContentType(QString(path).replace(QRegExp("^.*\\."), "").toLower(), response);
//    if (file.size() <= maxCachedFileSize && cacheOn) {
//        response->setHeader("Cache-Control", "max-age=" + QByteArray::number((int)(maxAge/1000)));
//        entry = new CacheEntry();
//        entry->created  = now;
//        entry->document = file.readAll();
//        response->write(entry->document);
//        cache.insert(path, entry, entry->document.size());
//    }

//    if (file.size() > maxCachedFileSize || !cacheOn) {
        response->write(file.readAll());
//    }

    file.close();
    return Status_Handled;
}

void StaticFileController::setContentType(const QString &ext, HttpResponse *response)
{
    QByteArray mime=mimeType(ext.toLatin1());
    if (!mime.isEmpty()) {
        response->setHeader("Content-Type", mime);
    }
}
