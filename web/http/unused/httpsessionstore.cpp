/**
 * @file
 *
 * @author Stefan Frings
 * @author Petr Bravenec petr.bravenec@hobrasoft.cz
 */

#include "httpsessionstore.h"
#include "httpsession.h"
#include "httpresponse.h"
#include "httprequest.h"
#include <QSettings>

static QString sessionCookieName;
static int sessionExpirationTime=-1;

HttpSessionStore::HttpSessionStore(QObject *parent) : QObject(parent) {
    if (-1==sessionExpirationTime) {
        QSettings settings;
        ::sessionCookieName=settings.value("http/sessionCookieName", "sessionid").toString();
        sessionExpirationTime=settings.value("http/sessionExpirationTime", 3600).toInt();
    }
    m_cleaner = new QTimer(this);
    m_cleaner->setInterval(30000);
    m_cleaner->setSingleShot(false);
    m_cleaner->start();
    connect(m_cleaner, SIGNAL(timeout()), this, SLOT(slotCleaner()));
}


const QString &HttpSessionStore::sessionCookieName() const {
    return m_sessionCookieName.isEmpty() ? ::sessionCookieName : m_sessionCookieName;
}


QByteArray HttpSessionStore::sessionId(HttpRequest *request, HttpResponse *response) {
    QByteArray sessionId = response->cookie(sessionCookieName()).value().toUtf8();
    if (sessionId.isEmpty()) {
        sessionId = request->cookie(sessionCookieName()).toUtf8();
        }

    if (sessionId.isEmpty() || !m_sessions.contains(sessionId)) {
        return QByteArray();
        }

    return sessionId;
}


HttpSession HttpSessionStore::session(HttpRequest *request, HttpResponse *response) {
    QByteArray id = sessionId(request, response);
    if (!id.isEmpty()) {
        HttpSession session = m_sessions.value(id);
        session.setLastAccess();
        return session;
        }

    HttpSession session;
    m_sessions.insert(session.id(), session);
    response->setCookie(
            HttpCookie(
                sessionCookieName(),
                session.id(),
                sessionExpirationTime,
                QByteArray(),
                QByteArray(),
                QByteArray()
                )
            );
    return session;
}


HttpSession HttpSessionStore::session(const QByteArray &id) {
    HttpSession session = m_sessions.value(id);
    session.setLastAccess();
    return session;
}


void HttpSessionStore::slotCleaner() {
    qint64 now = QDateTime::currentDateTime().toTime_t()*1000;
    QHash<QByteArray, HttpSession>::iterator i = m_sessions.begin() ;
    while (i != m_sessions.end()) {
        QHash<QByteArray, HttpSession>::iterator prev = i;
        i++;
        HttpSession session = prev.value();
        if (now - session.lastAccess() > sessionExpirationTime*1000) {
            m_sessions.erase(prev);
            }
        }
}


void HttpSessionStore::remove(HttpSession session) {
    m_sessions.remove(session.id());
}


