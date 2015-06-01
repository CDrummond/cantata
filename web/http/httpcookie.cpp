/**
 * @file
 *
 * @author Stefan Frings
 * @author Petr Bravenec petr.bravenec@hobrasoft.cz
 * @author Craig Drummond
 */

#include "httpcookie.h"

HttpCookie::HttpCookie(const QByteArray &nam, const QByteArray &val, const int ageMax,
                       const QByteArray &pth, const QByteArray &com, const QByteArray &dom,
                       const bool secure)
    : name(nam)
    , value(val)
    , comment(com)
    , domain(dom)
    , maxAge(ageMax)
    , path(pth)
    , isSecure(secure)
    , version(1)
{
}

QByteArray HttpCookie::toByteArray() const
{
    QByteArray buffer;
    buffer += name;
    buffer += "=";
    buffer += value;

    if (!comment.isEmpty()) {
        buffer += "; Comment=";
        buffer += comment;
    }

    if (!domain.isEmpty()) {
        buffer += "; Domain=";
        buffer += domain;
    }

    if (maxAge) {
        buffer += "; Max-Age=";
        buffer += QByteArray::number(maxAge);
    }

    if (!path.isEmpty()) {
        buffer += "; Path=";
        buffer += path;
    }

    if (isSecure) {
        buffer += "; Secure";
    }

    buffer += "; Version=";
    buffer += QByteArray::number(version);
    return buffer;
}
