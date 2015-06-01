/**
 * @file
 *
 * @author Stefan Frings
 * @author Petr Bravenec petr.bravenec@hobrasoft.cz
 */

#include "httpcookie.h"


HttpCookie::HttpCookie() {
    m_version = 1;
    m_maxAge  = 0;
    m_secure  = false;
}


HttpCookie::HttpCookie(
        const QString &name,
        const QString &value,
        const int maxAge, 
        const QString &path,
        const QString &comment,
        const QString &domain,
        const bool secure) {
    m_name    = name;
    m_value   = value;
    m_maxAge  = maxAge;
    m_path    = path;
    m_comment = comment;
    m_domain  = domain;
    m_secure  = secure;
    m_version = 1;
}


QByteArray HttpCookie::toByteArray() const {
    QString buffer;
    buffer += m_name;
    buffer += "=";
    buffer += m_value;

    if (!m_comment.isEmpty()) {
        buffer += "; Comment=";
        buffer += m_comment;
        }

    if (!m_domain.isEmpty()) {
        buffer += "; Domain=";
        buffer += m_domain;
        }

    if (m_maxAge != 0) {
        buffer += "; Max-Age=";
        buffer += QString("%1").arg(m_maxAge);
        }

    if (!m_path.isEmpty()) {
        buffer += "; Path=";
        buffer += m_path;
        }

    if (m_secure) {
        buffer += "; Secure";
        }

    buffer += "; Version=";
    buffer += QString("%1").arg(m_version);

    return buffer.toUtf8();
}

