/**
 * @file
 *
 * @author Stefan Frings
 * @author Petr Bravenec petr.bravenec@hobrasoft.cz
 * @author Craig Drummond
 */

#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <QTemporaryFile>
#include <QByteArray>
#include <QString>
#include <QMultiMap>

class QSettings;
class QTcpSocket;
class HttpConnection;

class HttpRequest
{
public:
    static void init(const QSettings *settings);

    enum Status {
        StatusWaitForRequest,
        StatusWaitForHeader,
        StatusWaitForBody,
        StatusComplete,
        StatusAbort
    };

    enum Method {
        Method_Unknown,
        Method_Get,
        Method_Post,
        Method_Put,
        Method_Delete
    };

    HttpRequest() { reset(); }

    void reset();
    void readFromSocket(QTcpSocket *socket);
    Status status() const { return stat; }
    Method method() const { return methd; }
    const QByteArray & path() const { return pth; }
    QByteArray header(const QByteArray &name) const { return hdrs.value(name); }
    QList<QByteArray>  headers(const QByteArray &name) const { return hdrs.values(name); }
    const QMultiMap<QByteArray, QByteArray> & headerMap() const { return hdrs; }
    QString parameter(const QByteArray &name) const { return QString::fromUtf8(params.value(name)); }
    QList<QByteArray> parameters(const QByteArray &name) const { return params.values(name); }
    const QMultiMap<QByteArray, QByteArray> & parameterMap() const { return params; }
    const QByteArray & body() const { return bodyData; }
    static QByteArray urlDecode(const QByteArray &source);
    QByteArray cookie(const QByteArray &name) const { return cookies.value(name); }
    const QMap<QByteArray, QByteArray> & cookieMap() { return cookies; }
    QTemporaryFile * file(const QByteArray &fieldName) { return files.value(fieldName); }

private:
    void parseMultiPartFile();
    void readRequest(QTcpSocket *socket);
    void readHeader(QTcpSocket *socket);
    void readBody(QTcpSocket *socket);
    void decodeRequestParams();
    void extractCookies();

private:
    QMultiMap<QByteArray, QByteArray> hdrs;
    QMultiMap<QByteArray, QByteArray> params;
    QMap<QByteArray, QTemporaryFile *> files;
    QMap<QByteArray, QByteArray> cookies;
    QByteArray bodyData;
    Method methd;
    QByteArray pth;
    Status stat;
    QByteArray boundary;
    int currentSize;
    int expectedBodySize;
    QByteArray currentHeader;
    QTemporaryFile tempFile;
};

#endif
