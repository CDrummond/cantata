/**
 * @file
 *
 * @author Stefan Frings
 * @author Petr Bravenec petr.bravenec@hobrasoft.cz
 */

#ifndef _HttpSessionStore_H_
#define _HttpSessionStore_H_

#include <QObject>
#include <QHash>
#include <QTimer>
#include <QString>
#include "httpsession.h"

class HttpRequest;
class HttpResponse;


/**
 * @brief Session store
 */
class HttpSessionStore : public QObject {
    Q_OBJECT
  public:

    /**
     * @brief Construktor
     */
    HttpSessionStore(QObject *parent);

    /**
     * @brief Returns session ID associated with the request or response
     */
    QByteArray sessionId(HttpRequest *request, HttpResponse *response);

    /**
     * @brief Returns session associated with the request or response
     */
    HttpSession session(HttpRequest *request, HttpResponse *response);

    /**
     * @brief Returns session 
     */
    HttpSession session(const QByteArray &id);

    /**
     * @brief Removes session
     */
    void remove(HttpSession session);

    /**
     * @brief Sets the name of the session cookie
     *
     * For some specific use it is needed to set some random cookie name for every session.
     */
    void setSessionCookieName(const QString &x) { m_sessionCookieName = x; }

    /**
     * @brief Returns the session cookie name
     */
    const QString &sessionCookieName() const;

  private slots:

    /**
     * @brief Slot cleans the store from old sessions
     */
    void    slotCleaner();

  private:
    QTimer  *m_cleaner;                         ///< Timer for store cleaning from old sessions
    QHash<QByteArray, HttpSession> m_sessions;  ///< List of stored sessions
    QString             m_sessionCookieName;    ///< The name of the session cookie

};

#endif
