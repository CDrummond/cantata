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

#ifndef ENCODERS_H
#define ENCODERS_H

#include <QString>
#include <QStringList>
#include <QList>

namespace Encoders
{
    extern QString constFfmpegPrefix;

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
        Encoder(const QString &n, const QString &d, const QString &t, const QString &e, const QString &a, const QString &c, const QString &f, const QString &v, const QList<Setting> &vs, const QString &l, const QString &h, int def)
            : name(n)
            , description(d)
            , tooltip(t)
            , extension(e)
            , app(a)
            , codec(c)
            , param(f)
            , valueLabel(v)
            , values(vs)
            , low(l)
            , high(h)
            , defaultValueIndex(def)
            , transcoder(true) {
        }
        bool isNull() { return name.isEmpty(); }
        bool operator==(const Encoder &o) const { return name==o.name; }
        QString changeExtension(const QString &file);
        bool isDifferent(const QString &file);
        QStringList params(int value, const QString &in, const QString &out) const;
        QString name;
        QString description;
        QString tooltip;
        QString extension;
        QString app;
        QString codec;
        QString param;
        QString valueLabel;
        QList<Setting> values;
        QString low;
        QString high;
        int defaultValueIndex;
        bool transcoder;
    };

    extern QList<Encoder> getAvailable();
    extern Encoder getEncoder(const QString &codec);
};

#endif
