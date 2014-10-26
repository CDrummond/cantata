/*
    Copyright 2006-2007 Kevin Ottens <ervin@kde.org>

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

#ifndef SOLID_SOLIDDEFS_P_H
#define SOLID_SOLIDDEFS_P_H

#include <QObject>

#define return_SOLID_CALL(Type, Object, Default, Method) \
    Type t = qobject_cast<Type>(Object); \
    if (t!=0) \
    { \
         return t->Method; \
    } \
    else \
    { \
         return Default; \
    }



#define SOLID_CALL(Type, Object, Method) \
    Type t = qobject_cast<Type>(Object); \
    if (t!=0) \
    { \
         t->Method; \
    }

//
// WARNING!!
// This code uses undocumented Qt API
// Do not copy it to your application! Use only the functions that are here!
// Otherwise, it could break when a new version of Qt ships.
//

/*
 * Lame copy of K_GLOBAL_STATIC below, but that's the price for
 * being completely kdecore independent.
 */

namespace Solid
{
    typedef void (*CleanUpFunction)();

    class CleanUpGlobalStatic
    {
    public:
        Solid::CleanUpFunction func;

        inline ~CleanUpGlobalStatic() { func(); }
    };
}

#ifdef Q_CC_MSVC
# define SOLID_GLOBAL_STATIC_STRUCT_NAME(NAME) _solid_##NAME##__LINE__
#else
# define SOLID_GLOBAL_STATIC_STRUCT_NAME(NAME)
#endif

#define SOLID_GLOBAL_STATIC(TYPE, NAME) SOLID_GLOBAL_STATIC_WITH_ARGS(TYPE, NAME, ())

#if QT_VERSION < 0x050000
#define SOLID_GLOBAL_STATIC_WITH_ARGS(TYPE, NAME, ARGS)                        \
static QBasicAtomicPointer<TYPE > _solid_static_##NAME = Q_BASIC_ATOMIC_INITIALIZER(0);\
static bool _solid_static_##NAME##_destroyed;                                  \
static struct SOLID_GLOBAL_STATIC_STRUCT_NAME(NAME)                            \
{                                                                              \
    bool isDestroyed()                                                         \
    {                                                                          \
        return _solid_static_##NAME##_destroyed;                               \
    }                                                                          \
    inline operator TYPE*()                                                    \
    {                                                                          \
        return operator->();                                                   \
    }                                                                          \
    inline TYPE *operator->()                                                  \
    {                                                                          \
        if (!_solid_static_##NAME) {                                           \
            if (isDestroyed()) {                                               \
                qFatal("Fatal Error: Accessed global static '%s *%s()' after destruction. " \
                       "Defined at %s:%d", #TYPE, #NAME, __FILE__, __LINE__);  \
            }                                                                  \
            TYPE *x = new TYPE ARGS;                                           \
            if (!_solid_static_##NAME.testAndSetOrdered(0, x)                  \
                && _solid_static_##NAME != x ) {                               \
                delete x;                                                      \
            } else {                                                           \
                static Solid::CleanUpGlobalStatic cleanUpObject = { destroy }; \
            }                                                                  \
        }                                                                      \
        return _solid_static_##NAME;                                           \
    }                                                                          \
    inline TYPE &operator*()                                                   \
    {                                                                          \
        return *operator->();                                                  \
    }                                                                          \
    static void destroy()                                                      \
    {                                                                          \
        _solid_static_##NAME##_destroyed = true;                               \
        TYPE *x = _solid_static_##NAME;                                        \
        _solid_static_##NAME = 0;                                              \
        delete x;                                                              \
    }                                                                          \
} NAME;

#else
#define SOLID_GLOBAL_STATIC_WITH_ARGS(TYPE, NAME, ARGS)                        \
static QBasicAtomicPointer<TYPE > _solid_static_##NAME = Q_BASIC_ATOMIC_INITIALIZER(0);\
static bool _solid_static_##NAME##_destroyed;                                  \
static struct SOLID_GLOBAL_STATIC_STRUCT_NAME(NAME)                            \
{                                                                              \
    bool isDestroyed()                                                         \
    {                                                                          \
        return _solid_static_##NAME##_destroyed;                               \
    }                                                                          \
    inline operator TYPE*()                                                    \
    {                                                                          \
        return operator->();                                                   \
    }                                                                          \
    inline TYPE *operator->()                                                  \
    {                                                                          \
        if (!_solid_static_##NAME.load()) {                                    \
            if (isDestroyed()) {                                               \
                qFatal("Fatal Error: Accessed global static '%s *%s()' after destruction. " \
                       "Defined at %s:%d", #TYPE, #NAME, __FILE__, __LINE__);  \
            }                                                                  \
            TYPE *x = new TYPE ARGS;                                           \
            if (!_solid_static_##NAME.testAndSetOrdered(0, x)                  \
                && _solid_static_##NAME.load() != x ) {                        \
                delete x;                                                      \
            } else {                                                           \
                static Solid::CleanUpGlobalStatic cleanUpObject = { destroy }; \
            }                                                                  \
        }                                                                      \
        return _solid_static_##NAME.load();                                    \
    }                                                                          \
    inline TYPE &operator*()                                                   \
    {                                                                          \
        return *operator->();                                                  \
    }                                                                          \
    static void destroy()                                                      \
    {                                                                          \
        _solid_static_##NAME##_destroyed = true;                               \
        TYPE *x = _solid_static_##NAME.load();                                 \
        _solid_static_##NAME.store(0);                                         \
        delete x;                                                              \
    }                                                                          \
} NAME;
#endif

#endif
