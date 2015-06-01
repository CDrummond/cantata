/**
 * @file
 *
 * @author Stefan Frings
 * @author Petr Bravenec petr.bravenec@hobrasoft.cz
 */

#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <QMap>
#include <QString>
#include <QTcpSocket>
#include "httpcookie.h"

class HttpConnection;

class HttpResponse
{
public:
    HttpResponse(HttpConnection *conn);

    void setHeader(const QByteArray &name, const QByteArray &value);
    void setHeader(const QByteArray &name, int value);
    void setSendHeaders(bool send) { sentHeaders = !send; }
    const QMap<QByteArray, QByteArray> & headers() const { return hdrs; }
    void clearHeaders() { hdrs.clear(); }
    QMap<QByteArray, HttpCookie> &cookies();
    HttpCookie cookie(const QByteArray &name) { return cookieMap.value(name); }
    void setStatus(int code, const QByteArray &text = QByteArray()) { statusCode=code; statusText=text; }
    void write(const QByteArray &data) { write(data, false); }
    void flush() { write(QByteArray(), true); }
    bool hasSentLastPart() const { return sentLastPart; }
    void setCookie(const HttpCookie &cookie);
    bool containsHeader(const QByteArray &name) { return hdrs.contains(name); }
    void complete();

private:
    bool isClosed() const { return closed; }
    void close() { closed=true; }
    void reinit();
    void writeToSocket(const QByteArray &data);
    void writeHeaders();
    void write(const QByteArray &data, bool lastPart);

private:
    HttpConnection * connection;
    QMap<QByteArray, QByteArray> hdrs;
    QMap<QByteArray, HttpCookie> cookieMap;
    int statusCode;
    QByteArray statusText;
    bool sentHeaders;
    bool sentLastPart;
    bool closed;

    friend class HttpConnection;
};

#endif
