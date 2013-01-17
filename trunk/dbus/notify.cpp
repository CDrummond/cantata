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

#include "notify.h"
#include "notificationsinterface.h"
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtCore/QCoreApplication>
#include <QtGui/QPixmap>
#include <QtGui/QImage>

QDBusArgument& operator<< (QDBusArgument &arg, const QImage &image)
{
    if (image.isNull()) {
        // Sometimes this gets called with a null QImage for no obvious reason.
        arg.beginStructure();
        arg << 0 << 0 << 0 << false << 0 << 0 << QByteArray();
        arg.endStructure();
        return arg;
    }
    QImage scaled = image.scaledToHeight(128, Qt::SmoothTransformation).convertToFormat(QImage::Format_ARGB32);

    #if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    // ABGR -> ARGB
    QImage i = scaled.rgbSwapped();
    #else
    // ABGR -> GBAR
    QImage i(scaled.size(), scaled.format());
    for (int y = 0; y < i.height(); ++y) {
        QRgb *p = (QRgb*) scaled.scanLine(y);
        QRgb *q = (QRgb*) i.scanLine(y);
        QRgb *end = p + scaled.width();
        while (p < end) {
            *q = qRgba(qGreen(*p), qBlue(*p), qAlpha(*p), qRed(*p));
            p++;
            q++;
        }
    }
    #endif

    arg.beginStructure();
    arg << i.width();
    arg << i.height();
    arg << i.bytesPerLine();
    arg << i.hasAlphaChannel();
    int channels = i.isGrayscale() ? 1 : (i.hasAlphaChannel() ? 4 : 3);
    arg << i.depth() / channels;
    arg << channels;
    arg << QByteArray(reinterpret_cast<const char*>(i.bits()), i.numBytes());
    arg.endStructure();
    return arg;
}

const QDBusArgument& operator>> (const QDBusArgument &arg, QImage &image)
{
  // This is needed to link but shouldn't be called.
  Q_ASSERT(0);
  Q_UNUSED(image);
  return arg;
}

static const int constTimeout=5000;

Notify::Notify(QObject *p)
    : QObject(p)
    , lastId(0)
{
    qDBusRegisterMetaType<QImage>();
    iface=new OrgFreedesktopNotificationsInterface(OrgFreedesktopNotificationsInterface::staticInterfaceName(),
                                                   "/org/freedesktop/Notifications", QDBusConnection::sessionBus());
}

void Notify::show(const QString &title, const QString &text, const QImage &img)
{
    QVariantMap hints;
    if (!img.isNull()) {
        hints["image_data"] = QVariant(img);
    }

    int id = 0;
    if (lastTime.secsTo(QDateTime::currentDateTime()) * 1000 < constTimeout) {
        // Reuse the existing popup if it's still open.  The reason we don't always
        // reuse the popup is because the notification daemon on KDE4 won't re-show
        // the bubble if it's already gone to the tray.  See issue #118
        id = lastId;
    }

    QDBusPendingReply<uint> reply = iface->Notify(QCoreApplication::applicationName(), id, "cantata", title, text, QStringList(), hints, constTimeout);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)), SLOT(callFinished(QDBusPendingCallWatcher*)));
}

void Notify::callFinished(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<uint> reply = *watcher;
    if (reply.isError()) {
        return;
    }

    uint id = reply.value();
    if (id != 0) {
        lastId = id;
        lastTime = QDateTime::currentDateTime();
    }
}

