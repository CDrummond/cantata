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

#include <QCoreApplication>
#include <QStringList>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include "tags.h"
#include "taghelper.h"

#ifdef Q_OS_WIN
#include <windows.h>
static long __stdcall exceptionHandler(EXCEPTION_POINTERS *p)
{
    Q_UNUSED(p)
    ::exit(0);
}
#endif

static QString logFileName;
static bool firstMsg=true;
#if QT_VERSION < 0x050000
static void cantataQtMsgHandler(QtMsgType, const char *msg)
#else
static void cantataQtMsgHandler(QtMsgType, const QMessageLogContext &, const QString &msg)
#endif
{
    QFile f(logFileName);
    if (f.open(QIODevice::WriteOnly|QIODevice::Append|QIODevice::Text)) {
        QTextStream stream(&f);
        if (firstMsg) {
            stream << "------------START------------" << endl;
            firstMsg=false;
        }
        stream << QDateTime::currentDateTime().toString(Qt::ISODate).replace("T", " ") << " - " << msg << endl;
    }
}

int main(int argc, char *argv[])
{
    #ifdef Q_OS_WIN
    // Prevent windows crash dialog from appearing...
    SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)exceptionHandler);
    #endif
    QCoreApplication app(argc, argv);
    if (3==app.arguments().length() || 4==app.arguments().length()) {
        if (4==app.arguments().length()) {
            logFileName=app.arguments().at(3);
            if (!logFileName.isEmpty()) {
                #if QT_VERSION < 0x050000
                qInstallMsgHandler(cantataQtMsgHandler);
                #else
                qInstallMessageHandler(cantataQtMsgHandler);
                #endif
                TagHelper::enableDebug();
                Tags::enableDebug();
            }
        }
        new TagHelper(app.arguments().at(1), app.arguments().at(2).toInt());
        return app.exec();
    }
    return 0;
}
