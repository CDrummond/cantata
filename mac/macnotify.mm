/*
 * Cantata
 *
 * Copyright (c) 2011-2016 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "macnotify.h"
#include "config.h"
#include <QString>
#include <QOperatingSystemVersion>
#include <QImage>
#include <QPixmap>
#include <Foundation/NSUserNotification.h>
#include <Cocoa/Cocoa.h>
#include <AppKit/NSApplication.h>

@interface UserNotificationItem : NSObject<NSUserNotificationCenterDelegate> { }

- (BOOL)userNotificationCenter:(NSUserNotificationCenter *)center shouldPresentNotification:(NSUserNotification *)notification;
@end

@implementation UserNotificationItem
- (BOOL)userNotificationCenter:(NSUserNotificationCenter *)center shouldPresentNotification:(NSUserNotification *)notification {
    Q_UNUSED(center);
    Q_UNUSED(notification);
    return YES;
}
@end

class UserNotificationItemClass
{
public:
    UserNotificationItemClass() {
        item = [UserNotificationItem alloc];
        if(QOperatingSystemVersion::current() >= QOperatingSystemVersion(QOperatingSystemVersion::MacOS, 10, 8)) {
            [[NSUserNotificationCenter defaultUserNotificationCenter] setDelegate:item];
        }
    }
    ~UserNotificationItemClass() {
        if(QOperatingSystemVersion::current() >= QOperatingSystemVersion(QOperatingSystemVersion::MacOS, 10, 8)) {
            [[NSUserNotificationCenter defaultUserNotificationCenter] setDelegate:nil];
        }
        [item release];
    }
    UserNotificationItem *item;
};

void MacNotify::showMessage(const QString &title, const QString &text, const QImage &img)
{
    if(QOperatingSystemVersion::current() >= QOperatingSystemVersion(QOperatingSystemVersion::MacOS, 10, 8)) {
        static UserNotificationItemClass *n=0;
        if (!n) {
            n=new UserNotificationItemClass();
        }

        NSUserNotification *userNotification = [[[NSUserNotification alloc] init] autorelease];
        userNotification.title = title.toNSString();
        userNotification.informativeText = text.toNSString();
        CGImageRef cg = img.toCGImage();
        NSImage *image = [[NSImage alloc] initWithCGImage:cg size:NSZeroSize];
        CFRelease(cg);
        userNotification.contentImage = image;
        [[NSUserNotificationCenter defaultUserNotificationCenter] deliverNotification:userNotification];
    }
}
