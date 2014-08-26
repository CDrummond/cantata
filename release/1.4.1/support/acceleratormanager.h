/*  This file is part of the KDE project
    Copyright (C) 2002 Matthias Hölzer-Klüpfel <mhk@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/


#ifndef K_ACCELERATORMANAGER_H
#define K_ACCELERATORMANAGER_H

#ifdef ENABLE_KDE_SUPPORT
namespace AcceleratorManager
{
    inline void manage(QWidget *) { }
};

#else

class QWidget;
class QString;

/**
 * KDE Accelerator manager.
 *
 * This class can be used to find a valid and working set of
 * accelerators for any widget.
 *
 * @author Matthias Hölzer-Klüpfel <mhk@kde.org>
*/

class AcceleratorManager
{
public:

    /**
     * Manages the accelerators of a widget.
     *
     * Call this function on the top widget of the hierarchy you
     * want to manage. It will fix the accelerators of the child widgets so
     * there are never duplicate accelerators. It also tries to put
     * accelerators on as many widgets as possible.
     *
     * The algorithm used tries to take the existing accelerators into
     * account, as well as the class of each widget. Hopefully, the result
     * is close to what you would assign manually.
     *
     * QPopupMenu's are managed dynamically, so when you add or remove entries,
     * the accelerators are reassigned. If you add or remove widgets to your
     * toplevel widget, you will have to call manage again to fix the
     * accelerators.
     *
     * @param widget The toplevel widget you want to manage.
     * @param programmers_mode if true, AcceleratorManager adds (&) for removed
     *             accels and & before added accels
     */

    static void manage(QWidget *widget, bool programmers_mode = false );

    /** \internal returns the result of the last manage operation. */
    static void last_manage(QString &added,  QString &changed,  QString &removed);

    /**
     * Use this method for a widget (and its children) you want no accels to be set on.
     */
    static void setNoAccel( QWidget *widget );
};
#endif

#endif // K_ACCELERATORMANAGER_H
