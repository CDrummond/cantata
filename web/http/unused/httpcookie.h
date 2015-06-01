/**
 * @file
 *
 * @author Stefan Frings
 * @author Petr Bravenec petr.bravenec@hobrasoft.cz
 */

#ifndef _HttpCookie_H_
#define _HttpCookie_H_

#include <QString>
#include <QByteArray>

/**
 * @brief One cookie of HTTP protocol
 */
class HttpCookie {
  public:

    /**
     * @brief Constructor, creates an empty cookie
     */
    HttpCookie();

    /**
     * @brief Constructor, creates new cookie with given parameters
     */
    HttpCookie( const QString &name,
                const QString &value,
                const int maxAge, 
                const QString &path = "/",
                const QString &comment = QString(),
                const QString &domain = QString(),
                const bool secure = false);

    /**
     * @brief Returns cookie on form of line formated for HTTP header
     */
    QByteArray toByteArray() const;

    void    setName     (const QString &x) { m_name = x; }      ///< Sets the name of cookie
    void    setValue    (const QString &x) { m_value = x; }     ///< Sets the value of cookie
    void    setComment  (const QString &x) { m_comment = x; }   ///< Sets the comment of cookie
    void    setDomain   (const QString &x) { m_domain = x; }    ///< Sets the domain of cookie
    void    setMaxAge   (const int &    x) { m_maxAge = x; }    ///< Sets the max age of cookie
    void    setPath     (const QString &x) { m_path = x; }      ///< Sets the path of cookie
    void    setSecure   (const bool &   x) { m_secure = x; }    ///< Sets the cookie as secure
    void    setVersion  (const int &    x) { m_version = x; }   ///< Sets the version ot cookie

    QString name()    const { return m_name; }      ///< Returns the name of cookie
    QString value()   const { return m_value; }     ///< Returns the value of cookie
    QString comment() const { return m_comment; }   ///< Returns the comment of cookie
    QString domain()  const { return m_domain; }    ///< Returns the domain of cookie
    int     maxAge()  const { return m_maxAge; }    ///< Returns the max age of cookie
    QString path()    const { return m_path; }      ///< Returns the path of cookie
    bool    secure()  const { return m_secure; }    ///< Returns the secure status of cookie
    int     version() const { return m_version; }   ///< Returns the version of cookie

  private:
    #ifndef DOXYGEN_SHOULD_SKIP_THIS
    QString m_name;
    QString m_value;
    QString m_comment;
    QString m_domain;
    int     m_maxAge;
    QString m_path;
    bool    m_secure;
    int     m_version;
    #endif
};

#endif
