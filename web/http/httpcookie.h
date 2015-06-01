/**
 * @file
 *
 * @author Stefan Frings
 * @author Petr Bravenec petr.bravenec@hobrasoft.cz
 * @author Craig Drummond
 */

#ifndef HTTP_COOKIE_H
#define HTTP_COOKIE_H

#include <QByteArray>

struct HttpCookie
{
    HttpCookie(const QByteArray &nam=QByteArray(),
               const QByteArray &val=QByteArray(),
               const int ageMax=0,
               const QByteArray &pth = "/",
               const QByteArray &com = QByteArray(),
               const QByteArray &dom = QByteArray(),
               const bool secure = false);

    QByteArray toByteArray() const;

    QByteArray name;
    QByteArray value;
    QByteArray comment;
    QByteArray domain;
    int     maxAge;
    QByteArray path;
    bool    isSecure;
    int     version;
};

#endif
