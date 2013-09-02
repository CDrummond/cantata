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

#ifndef LOCALIZE_H
#define LOCALIZE_H

#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocalizedString>
#else
#include <QObject>

inline QString i18n(const char *text)
{
    return QObject::tr(text);
}

template <typename A1>
inline QString i18n(const char *text, const A1 &a1)
{
    return QObject::tr(text).arg(a1);
}

template <typename A1, typename A2>
inline QString i18n(const char *text, const A1 &a1, const A2 &a2)
{
    return QObject::tr(text).arg(a1).arg(a2);
}

template <typename A1, typename A2, typename A3>
inline QString i18n(const char *text, const A1 &a1, const A2 &a2, const A3 &a3)
{
    return QObject::tr(text).arg(a1).arg(a2).arg(a3);
}

template <typename A1, typename A2, typename A3, typename A4>
inline QString i18n(const char *text, const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4)
{
    return QObject::tr(text).arg(a1).arg(a2).arg(a3).arg(a4);
}

template <typename A1, typename A2, typename A3, typename A4, typename A5>
inline QString i18n(const char *text, const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4, const A5 &a5)
{
    return QObject::tr(text).arg(a1).arg(a2).arg(a3).arg(a4).arg(a5);
}

template <typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
inline QString i18n(const char *text, const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4, const A5 &a5, const A6 &a6)
{
    return QObject::tr(text).arg(a1).arg(a2).arg(a3).arg(a4).arg(a5).arg(a6);
}

template <typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7>
inline QString i18n(const char *text, const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4, const A5 &a5, const A6 &a6, const A7 &a7)
{
    return QObject::tr(text).arg(a1).arg(a2).arg(a3).arg(a4).arg(a5).arg(a6).arg(a7);
}

template <typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8>
inline QString i18n(const char *text, const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4, const A5 &a5, const A6 &a6, const A7 &a7, const A8 &a8)
{
    return QObject::tr(text).arg(a1).arg(a2).arg(a3).arg(a4).arg(a5).arg(a6).arg(a7).arg(a8);
}

template <typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9>
inline QString i18n(const char *text, const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4, const A5 &a5, const A6 &a6, const A7 &a7, const A8 &a8, const A9 &a9)
{
    return QObject::tr(text).arg(a1).arg(a2).arg(a3).arg(a4).arg(a5).arg(a6).arg(a7).arg(a8).arg(a9);
}

inline QString i18nc(const char *ctxt, const char *text)
{
    return QObject::tr(text, ctxt);
}

template <typename A1>
inline QString i18nc(const char *ctxt, const char *text, const A1 &a1)
{
    return QObject::tr(text, ctxt).arg(a1);
}

template <typename A1, typename A2>
inline QString i18nc(const char *ctxt, const char *text, const A1 &a1, const A2 &a2)
{
    return QObject::tr(text, ctxt).arg(a1).arg(a2);
}

template <typename A1, typename A2, typename A3>
inline QString i18nc(const char *ctxt, const char *text, const A1 &a1, const A2 &a2, const A3 &a3)
{
    return QObject::tr(text, ctxt).arg(a1).arg(a2).arg(a3);
}

template <typename A1, typename A2, typename A3, typename A4>
inline QString i18nc(const char *ctxt, const char *text, const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4)
{
    return QObject::tr(text, ctxt).arg(a1).arg(a2).arg(a3).arg(a4);
}

template <typename A1, typename A2, typename A3, typename A4, typename A5>
inline QString i18nc(const char *ctxt, const char *text, const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4, const A5 &a5)
{
    return QObject::tr(text, ctxt).arg(a1).arg(a2).arg(a3).arg(a4).arg(a5);
}

template <typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
inline QString i18nc(const char *ctxt, const char *text, const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4, const A5 &a5, const A6 &a6)
{
    return QObject::tr(text, ctxt).arg(a1).arg(a2).arg(a3).arg(a4).arg(a5).arg(a6);
}

template <typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7>
inline QString i18nc(const char *ctxt, const char *text, const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4, const A5 &a5, const A6 &a6, const A7 &a7)
{
    return QObject::tr(text, ctxt).arg(a1).arg(a2).arg(a3).arg(a4).arg(a5).arg(a6).arg(a7);
}

template <typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8>
inline QString i18nc(const char *ctxt, const char *text, const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4, const A5 &a5, const A6 &a6, const A7 &a7, const A8 &a8)
{
    return QObject::tr(text, ctxt).arg(a1).arg(a2).arg(a3).arg(a4).arg(a5).arg(a6).arg(a7).arg(a8);
}

template <typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9>
inline QString i18nc(const char *ctxt, const char *text, const A1 &a1, const A2 &a2, const A3 &a3, const A4 &a4, const A5 &a5, const A6 &a6, const A7 &a7, const A8 &a8, const A9 &a9)
{
    return QObject::tr(text, ctxt).arg(a1).arg(a2).arg(a3).arg(a4).arg(a5).arg(a6).arg(a7).arg(a8).arg(a9);
}

#endif

#endif
