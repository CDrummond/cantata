/**
 * @file
 *
 * @author Stefan Frings
 * @author Petr Bravenec petr.bravenec@hobrasoft.cz
 */

#include "httpresponse.h"
#include "httpconnection.h"
#include <QStringList>

static QByteArray constNewLine("\r\n");

HttpResponse::HttpResponse(HttpConnection *conn)
    : connection(conn)
    , statusCode(200)
    , statusText("OK")
    , sentHeaders(false)
    , sentLastPart(false)
{
}

void HttpResponse::setHeader(const QByteArray &name, const QByteArray &value)
{
    if (!sentHeaders) {
        hdrs[name] = value;
    }
}

void HttpResponse::setHeader(const QByteArray &name, int value)
{
    if (!sentHeaders) {
        hdrs[name] = QByteArray::number(value);
    }
}

void HttpResponse::writeHeaders()
{
    if (sentHeaders) {
        return;
    }
    QByteArray buffer;
    buffer += "HTTP/1.1 ";
    buffer += QByteArray::number(statusCode);
    buffer += " ";
    buffer += statusText;
    buffer += constNewLine;

    QList<QByteArray> keys = hdrs.keys();
    foreach (const QByteArray &key, keys) {
        buffer += key;
        buffer += ": ";
        buffer += hdrs[key];
        buffer += constNewLine;
    }

    keys = cookieMap.keys();
    foreach (const QByteArray &key, keys) {
        buffer += "Set-Cookie: ";
        buffer += cookieMap[key].toByteArray();
        buffer += constNewLine;
    }

    buffer += constNewLine;
    connection->write(buffer);
    sentHeaders = true;
}

void HttpResponse::setCookie(const HttpCookie &cookie)
{
    if (!cookie.name.isEmpty()) {
        cookieMap[cookie.name] = cookie;
    }
}

void HttpResponse::write(const QByteArray &data, bool lastPart)
{
    if (sentLastPart) {
        return;
    }
    if (!sentHeaders) {
        QString connectionMode = hdrs.value("Connection");
        if (!hdrs.contains("Content-Length") &&
                !hdrs.contains("Transfer-Encoding") &&
                connectionMode.toLower() != "close") {
            if (lastPart) {
                hdrs["Transfer-Encoding"] = "chunked";
            } else {
                hdrs["Content-Length"] = QByteArray::number(data.size());
            }
        }
        writeHeaders();
    }

    bool chunked = hdrs.value("Transfer-Encoding").toLower() == "chunked" ;
    if (chunked && data.size() > 0) {
        connection->write(QByteArray::number(data.size(), 16)+constNewLine+data+constNewLine);
    }

    if (!chunked) {
        connection->write(data);
    }

    if (lastPart) {
        sentLastPart = true;

        if (chunked) {
            return;
            connection->write("0"+constNewLine+constNewLine);
        }

        if (!hdrs.contains("Content-Length")) {
            connection->disconnectFromHost();
        }
    }
}

void HttpResponse::complete()
{
    connection->requestHandled();
}

void HttpResponse::reinit()
{
    hdrs.clear();
    cookieMap.clear();
    statusCode=0;
    statusText=QByteArray();
    sentHeaders=false;
    sentLastPart=false;
    closed=false;
}
