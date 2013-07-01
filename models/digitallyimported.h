/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
 *
 * ----
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef DIGITALLYIMPORTED_H
#define DIGITALLYIMPORTED_H

#include <QObject>
#include <QDateTime>

class QNetworkRequest;
class QNetworkReply;

class DigitallyImported : public QObject
{
    Q_OBJECT

public:
    static const QString constApiUserName;
    static const QString constApiPassword;
    static const QString constPublicValue;
    static DigitallyImported * self();

    DigitallyImported();
    ~DigitallyImported();

    void addAuthHeader(QNetworkRequest &req) const;
    void load();
    void save();
    bool haveAccount() const { return !userName.isEmpty() && !password.isEmpty(); }
    bool loggedIn() const { return !listenHash.isEmpty(); }

    const QString & user() const { return userName; }
    const QString & pass() const { return password; }
    int audioType() const { return streamType; }
    const QDateTime & sessionExpiry() const { return expires; }
    void setUser(const QString &u) { userName=u; }
    void setPass(const QString &p) { password=p; }
    void setAudioType(int a) { streamType=a; }

    const QString & statusString() const { return status; }

    QString modifyUrl(const QString &u) const;

public Q_SLOTS:
    void login();

Q_SIGNALS:
    void loginStatus(bool ok, const QString &msg);

private Q_SLOTS:
    void loginResponse();

private:
    QNetworkReply *job;
    QString status;
    QString userName;
    QString password;
    QString listenHash;
    QDateTime expires;
    int streamType;
};

#endif
