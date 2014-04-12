/*
 * Copyright 2012 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import QtQuick 2.0
import Ubuntu.Components 0.1

// internal helper class for text inside the list items.
Label {
    id: label
    property bool selected: false
    property bool secondary: false

    // FIXME: very ugly hack to detect whether the list item is inside a Popover
    property bool overlay: isInsideOverlay(label)
    function isInsideOverlay(item) {
        if (!item.parent) return false;
        return item.parent.hasOwnProperty("pointerTarget") || label.isInsideOverlay(item.parent)
    }

    fontSize: "medium"
    elide: Text.ElideRight
    color: selected ? UbuntuColors.orange : secondary ? overlay ? Theme.palette.normal.overlayText : Theme.palette.normal.backgroundText
                                                      : overlay ? Theme.palette.selected.overlayText : Theme.palette.selected.backgroundText
    opacity: label.enabled ? 1.0 : 0.5
}
