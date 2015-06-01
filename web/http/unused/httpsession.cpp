/**
 * @file
 *
 * @author Stefan Frings
 * @author Petr Bravenec petr.bravenec@hobrasoft.cz
 */

#include "httpsession.h"
#include <QUuid>
#include <QDebug>


HttpSession::~HttpSession() {
    if (m_data == NULL) return;
    if ( --(m_data->refCount) == 0) {
        delete m_data;
        }
}


HttpSession::HttpSession() {
    m_data = new HttpSessionData();
    m_data->refCount=1;
    m_data->lastAccess = QDateTime::currentDateTime().toTime_t()*1000;
    m_data->id = QUuid::createUuid().toString().toLatin1();
}


HttpSession::HttpSession(const HttpSession &other) {
    m_data = other.m_data;
    if (m_data) {
        m_data->refCount++;
        }
}


HttpSession &HttpSession::operator= (const HttpSession &other) {
    m_data = other.m_data;
    if (m_data) {
        m_data->refCount++;
        m_data->lastAccess = QDateTime::currentDateTime().toTime_t()*1000;
        }

    return *this;
}

