/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef ENCODERS_H
#define ENCODERS_H

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QList>

namespace Encoders
{
    struct Setting {
        Setting(const QString &d, int v)
            : descr(d)
            , value(v) {
        }
        QString descr;
        int value;
    };

    struct Encoder {
        Encoder()
            : defaultValueIndex(0) {
        }
        Encoder(const QString &n, const QString &d, const QString &t, const QString &e, const QString &c, const QString &f, const QString &v, const QList<Setting> &vs, const QString &l, const QString &h, int def)
            : name(n)
            , description(d)
            , tooltip(t)
            , extension(e)
            , codec(c)
            , ffmpegParam(f)
            , valueLabel(v)
            , values(vs)
            , low(l)
            , high(h)
            , defaultValueIndex(def) {
        }
        bool isNull() {
            return name.isEmpty();
        }
        QString changeExtension(const QString &file);
        bool isDifferent(const QString &file);
        QStringList params(int value, const QString &in, const QString &out) const;
        QString name;
        QString description;
        QString tooltip;
        QString extension;
        QString codec;
        QString ffmpegParam;
        QString valueLabel;
        QList<Setting> values;
        QString low;
        QString high;
        int defaultValueIndex;
    };

    extern QList<Encoder> getAvailable();
    extern Encoder getEncoder(const QString &codec);
};

#endif
