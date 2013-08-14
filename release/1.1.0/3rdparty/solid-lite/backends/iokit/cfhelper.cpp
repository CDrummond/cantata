/*
    Copyright 2009 Harald Fernengel <harry@kdevelop.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) version 3, or any
    later version accepted by the membership of KDE e.V. (or its
    successor approved by the membership of KDE e.V.), which shall
    act as a proxy defined in Section 6 of version 3 of the license.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library. If not, see <http://www.gnu.org/licenses/>.
*/

#include <qvarlengtharray.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qvariant.h>
#include <qdatetime.h>
#include <qdebug.h>

#include <CoreFoundation/CoreFoundation.h>

/* helper classes to convert from CF types to Qt */

static QString q_toString(const CFStringRef &str)
{
    CFIndex length = CFStringGetLength(str);
    QVarLengthArray<UniChar> buffer(length);

    CFRange range = { 0, length };
    CFStringGetCharacters(str, range, buffer.data());
    return QString(reinterpret_cast<const QChar *>(buffer.data()), length);
}

template <typename T>
static inline T convertCFNumber(const CFNumberRef &num, CFNumberType type)
{
    T n;
    CFNumberGetValue(num, type, &n);
    return n;
}

static QVariant q_toVariant(const CFTypeRef &obj)
{
    const CFTypeID typeId = CFGetTypeID(obj);

    if (typeId == CFStringGetTypeID())
        return QVariant(q_toString(static_cast<const CFStringRef>(obj)));

    if (typeId == CFNumberGetTypeID()) {
        const CFNumberRef num = static_cast<const CFNumberRef>(obj);
        const CFNumberType type = CFNumberGetType(num);
        switch (type) {
        case kCFNumberSInt8Type:
            return qVariantFromValue(convertCFNumber<char>(num, type));
        case kCFNumberSInt16Type:
            return qVariantFromValue(convertCFNumber<qint16>(num, type));
        case kCFNumberSInt32Type:
            return qVariantFromValue(convertCFNumber<qint32>(num, type));
        case kCFNumberSInt64Type:
            return qVariantFromValue(convertCFNumber<qint64>(num, type));
        case kCFNumberCharType:
            return qVariantFromValue(convertCFNumber<uchar>(num, type));
        case kCFNumberShortType:
            return qVariantFromValue(convertCFNumber<short>(num, type));
        case kCFNumberIntType:
            return qVariantFromValue(convertCFNumber<int>(num, type));
        case kCFNumberLongType:
            return qVariantFromValue(convertCFNumber<long>(num, type));
        case kCFNumberLongLongType:
            return qVariantFromValue(convertCFNumber<long long>(num, type));
        case kCFNumberFloatType:
            return qVariantFromValue(convertCFNumber<float>(num, type));
        case kCFNumberDoubleType:
            return qVariantFromValue(convertCFNumber<double>(num, type));
        default:
            if (CFNumberIsFloatType(num))
                return qVariantFromValue(convertCFNumber<double>(num, kCFNumberDoubleType));
            return qVariantFromValue(convertCFNumber<quint64>(num, kCFNumberLongLongType));
        }
    }

    if (typeId == CFDateGetTypeID()) {
        QDateTime dt;
        dt.setTime_t(uint(kCFAbsoluteTimeIntervalSince1970));
        return dt.addSecs(int(CFDateGetAbsoluteTime(static_cast<const CFDateRef>(obj))));
    }

    if (typeId == CFDataGetTypeID()) {
        const CFDataRef cfdata = static_cast<const CFDataRef>(obj);
        return QByteArray(reinterpret_cast<const char *>(CFDataGetBytePtr(cfdata)),
                    CFDataGetLength(cfdata));
    }

    if (typeId == CFBooleanGetTypeID())
        return QVariant(bool(CFBooleanGetValue(static_cast<const CFBooleanRef>(obj))));

    if (typeId == CFArrayGetTypeID()) {
        const CFArrayRef cfarray = static_cast<const CFArrayRef>(obj);
        QList<QVariant> list;
        CFIndex size = CFArrayGetCount(cfarray);
        bool metNonString = false;
        for (CFIndex i = 0; i < size; ++i) {
            QVariant value = q_toVariant(CFArrayGetValueAtIndex(cfarray, i));
            if (value.type() != QVariant::String)
                metNonString = true;
            list << value;
        }
        if (metNonString)
            return list;
        else
            return QVariant(list).toStringList();
    }

    if (typeId == CFDictionaryGetTypeID()) {
        const CFDictionaryRef cfdict = static_cast<const CFDictionaryRef>(obj);
        const CFTypeID arrayTypeId = CFArrayGetTypeID();
        int size = int(CFDictionaryGetCount(cfdict));
        QVarLengthArray<CFPropertyListRef> keys(size);
        QVarLengthArray<CFPropertyListRef> values(size);
        CFDictionaryGetKeysAndValues(cfdict, keys.data(), values.data());

        QMultiMap<QString, QVariant> map;
        for (int i = 0; i < size; ++i) {
            QString key = q_toString(static_cast<const CFStringRef>(keys[i]));

            if (CFGetTypeID(values[i]) == arrayTypeId) {
                const CFArrayRef cfarray = static_cast<const CFArrayRef>(values[i]);
                CFIndex arraySize = CFArrayGetCount(cfarray);
                for (CFIndex j = arraySize - 1; j >= 0; --j)
                    map.insert(key, q_toVariant(CFArrayGetValueAtIndex(cfarray, j)));
            } else {
                map.insert(key, q_toVariant(values[i]));
            }
        }
        return map;
    }

    return QVariant();
}

QMap<QString, QVariant> q_toVariantMap (const CFMutableDictionaryRef &dict)
{
    Q_ASSERT(dict);

    QMap<QString, QVariant> result;

    const int count = CFDictionaryGetCount(dict);
    QVarLengthArray<void *> keys(count);
    QVarLengthArray<void *> values(count);

    CFDictionaryGetKeysAndValues(dict,
            const_cast<const void **>(keys.data()),
            const_cast<const void **>(values.data()));

    for (int i = 0; i < count; ++i) {
        const QString key = q_toString((CFStringRef)keys[i]);
        const QVariant value = q_toVariant((CFTypeRef)values[i]);
        result[key] = value;
    }

    return result;
}

