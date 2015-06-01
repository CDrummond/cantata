/**
 * @file
 *
 * @author Stefan Frings
 * @author Petr Bravenec petr.bravenec@hobrasoft.cz
 * @author Craig Drummond
 */

#include "httprequest.h"
#include "httpconnection.h"
#include "basichttpserver.h"
#include <QList>
#include <QDir>
#include <QSettings>
#include <QDebug>

#define DBUG if (BasicHttpServer::debugEnabled()) qWarning() << "HttpRequest" << (void *)this << __FUNCTION__

static int maxRequestSize=16384;
static int maxMultiPartSize=1048576;

void HttpRequest::init(const QSettings *settings)
{
    maxRequestSize=settings->value("http/maxRequestSize", maxRequestSize).toInt();
    maxMultiPartSize=settings->value("http/maxMultiPartSize", maxMultiPartSize).toInt();
}

void HttpRequest::reset()
{
    stat=StatusWaitForRequest;
    currentSize=0;
    expectedBodySize=0;
    methd=Method_Unknown;
}

void HttpRequest::readFromSocket(QTcpSocket *socket)
{
    switch(stat) {
    case StatusComplete:
        return;
        break;
    case StatusAbort:
        return;
        break;
    case StatusWaitForRequest:
        readRequest(socket);
        break;
    case StatusWaitForHeader:
        readHeader(socket);
        break;
    case StatusWaitForBody:
        readBody(socket);
        break;
    }
    if (currentSize > maxRequestSize) {
        stat = StatusAbort;
    }
    if (StatusComplete==stat) {
        decodeRequestParams();
        extractCookies();
    }
}

void HttpRequest::readRequest(QTcpSocket *socket)
{
    int toRead = maxRequestSize - currentSize + 1;
    QByteArray newdata = socket->readLine(toRead).trimmed();
    currentSize += newdata.size();

    if (newdata.isEmpty()) {
        return;
    }

    QList<QByteArray> list = newdata.split(' ');
    if (list.size() != 3 || !list[2].contains("HTTP")) {
        DBUG << "received broken HTTP request, invalid first line";
        stat = StatusAbort;
        return;
    }

    QByteArray m=list[0];
    if ("GET"==m) {
        methd=Method_Get;
    } else if ("POST"==m) {
        methd=Method_Post;
    } else if ("PUT"==m) {
        methd=Method_Put;
    } else if ("DELETE"==m) {
        methd=Method_Delete;
    }
    pth = urlDecode(list[1]);
    stat = StatusWaitForHeader;
}

void HttpRequest::readHeader(QTcpSocket *socket)
{
    int toRead = maxRequestSize - currentSize + 1;
    QByteArray newdata = socket->readLine(toRead).trimmed();
    currentSize += newdata.size();

    int colonpos = newdata.indexOf(':');
    if (colonpos > 0) {
        currentHeader = newdata.left(colonpos);
        QByteArray value = newdata.mid(colonpos+1).trimmed();
        hdrs.insert(currentHeader, value);
        return;
    }

    if (colonpos <= 0 && !newdata.isEmpty()) {
        hdrs.insert(currentHeader, hdrs.value(currentHeader)+" "+newdata);
        return;
    }

    if (newdata.isEmpty()) {
        QByteArray contentType = hdrs.value("Content-Type");

        if (contentType.startsWith("multipart/form-data")) {
            int pos  = contentType.indexOf("boundary=");
            if (pos >= 0) {
                boundary = contentType.mid(pos+9);
            }
        }

        expectedBodySize = hdrs.value("Content-Length").toInt();
        if (expectedBodySize <= 0) {
            stat = StatusComplete;
            return;
        }

        if (boundary.isEmpty() && expectedBodySize + currentSize > maxRequestSize) {
            DBUG << "expected body is too large";
            stat = StatusAbort;
            return;
        }

        if (!boundary.isEmpty() && expectedBodySize > maxMultiPartSize) {
            DBUG << "expected multipart body is too large";
            stat = StatusAbort;
            return;
        }

        stat = StatusWaitForBody;
    }
}

void HttpRequest::readBody(QTcpSocket *socket)
{
    // normal body, no multipart
    if (boundary.isEmpty()) {
        int toRead = expectedBodySize - bodyData.size();
        QByteArray newdata = socket->read(toRead);
        currentSize += newdata.size();

        bodyData.append(newdata);
        if (bodyData.size() >= expectedBodySize) {
            stat = StatusComplete;
        }
        return;
    }

    if (!boundary.isEmpty()) {
        if (!tempFile.isOpen()) {
            tempFile.open();
        }
        int filesize = tempFile.size();
        int toRead = expectedBodySize - filesize;
        if (toRead > 65536) {
            toRead = 65536;
        }

        filesize += tempFile.write(socket->read(toRead));
        if (filesize >= maxMultiPartSize) {
            DBUG << "received too many multipart bytes";
            stat = StatusAbort;
            return;
        }

        if (filesize >= expectedBodySize) {
            tempFile.flush();
            parseMultiPartFile();
            tempFile.close();
            stat = StatusComplete;
        }
    }
}

void HttpRequest::decodeRequestParams()
{
    QByteArray rawParameters;
    if (hdrs.value("Content-Type") == "application/x-www-form-urlencoded") {
        rawParameters = bodyData;
    } else {
        int questionmarkpos = pth.indexOf('?');
        if (questionmarkpos >= 0) {
            rawParameters = pth.mid(questionmarkpos+1);
            pth = pth.left(questionmarkpos);
        }
    }

    QList<QByteArray> list = rawParameters.split('&');
    foreach (const QByteArray &part, list) {
        int eqpos = part.indexOf('=');
        if (eqpos >= 0) {
            QByteArray name  = urlDecode(part.left(eqpos).trimmed());
            QByteArray value = urlDecode(part.mid(eqpos+1).trimmed());
            params.insert(name, value);
            continue;
        }

        if (!part.isEmpty()) {
            QByteArray name = urlDecode(part);
            params.insert(name, "");
        }
    }
}

void HttpRequest::extractCookies()
{
    QList<QByteArray> cookieList = hdrs.values("Cookie");
    foreach (const QByteArray &cook, cookieList) {
        QList<QByteArray> parts = cook.split(';');
        foreach (const QByteArray &part, parts) {
            int eqpos = part.indexOf('=');
            if (eqpos >= 0) {
                QByteArray name = part.left(eqpos).trimmed();
                if (name.startsWith('$')) {
                    continue;
                }
                cookies.insert(name, part.mid(eqpos+1).trimmed());
                continue;
            }
            if (eqpos < 0) {
                cookies.insert(part, "");
                continue;
            }
        }
    }
    hdrs.remove("Cookie");
}

QByteArray HttpRequest::urlDecode(const QByteArray &text)
{
    QByteArray buffer(text);
    buffer.replace('+', ' ');
    int percentPos=buffer.indexOf('%');
    while (percentPos>=0) {
        bool ok;
        char byte=buffer.mid(percentPos+1, 2).toInt(&ok, 16);
        if (ok) {
            buffer.replace(percentPos, 3, (char*)&byte, 1);
        }
        percentPos=buffer.indexOf('%', percentPos+1);
    }
    return buffer;

}

void HttpRequest::parseMultiPartFile()
{
    DBUG << "parsing multipart temp file";
    tempFile.seek(0);
    bool finished=false;
    while (!tempFile.atEnd() && !finished && !tempFile.error()) {
        QByteArray fieldName;
        QByteArray fileName;
        while (!tempFile.atEnd() && !finished && !tempFile.error()) {
            QByteArray line=tempFile.readLine(65536).trimmed();
            if (line.startsWith("Content-Disposition:")) {
                if (line.contains("form-data")) {
                    int start=line.indexOf(" name=\"");
                    int end=line.indexOf("\"",start+7);
                    if (start>=0 && end>=start) {
                        fieldName=line.mid(start+7,end-start-7);
                    }
                    start=line.indexOf(" filename=\"");
                    end=line.indexOf("\"",start+11);
                    if (start>=0 && end>=start) {
                        fileName=line.mid(start+11,end-start-11);
                    }
                } else {
                    DBUG << "ignoring unsupported content part" << line.data();
                }
            } else if (line.isEmpty()) {
                break;
            }
        }

        QTemporaryFile *uploadedFile=0;
        QByteArray fieldValue;
        while (!tempFile.atEnd() && !finished && !tempFile.error()) {
            QByteArray line = tempFile.readLine(65536);
            if (line.startsWith("--"+boundary)) {
                // Boundary found. Until now we have collected 2 bytes too much,
                // so remove them from the last result
                if (fileName.isEmpty() && !fieldName.isEmpty()) {
                    // last field was a form field
                    fieldValue.remove(fieldValue.size()-2,2);
                    params.insert(fieldName,fieldValue);
                    DBUG << "set parameter" << fieldName.data() << fieldValue.data();
                }
                else if (!fileName.isEmpty() && !fieldName.isEmpty()) {
                    // last field was a file
                    uploadedFile->resize(uploadedFile->size()-2);
                    uploadedFile->flush();
                    uploadedFile->seek(0);
                    params.insert(fieldName,fileName);
                    DBUG << "set parameter" << fieldName.data() << fileName.data();
                    files.insert(fieldName,uploadedFile);
                    DBUG << "uploaded file size is" << uploadedFile->size();
                }
                if (line.contains(boundary+"--")) {
                    finished=true;
                }
                break;
            } else {
                if (fileName.isEmpty() && !fieldName.isEmpty()) {
                    // this is a form field.
                    currentSize+=line.size();
                    fieldValue.append(line);
                } else if (!fileName.isEmpty() && !fieldName.isEmpty()) {
                    // this is a file
                    if (!uploadedFile) {
                        uploadedFile=new QTemporaryFile();
                        uploadedFile->open();
                    }
                    uploadedFile->write(line);
                    if (uploadedFile->error()) {
                        qCritical("HttpRequest: error writing temp file, %s", qPrintable(uploadedFile->errorString()));
                    }
                }
            }
        }
    }
    if (tempFile.error()) {
        qCritical("HttpRequest: cannot read temp file, %s", qPrintable(tempFile.errorString()));
    }
}
