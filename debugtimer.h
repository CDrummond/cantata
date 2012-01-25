#ifndef DEBUG_TIMER
#define DEBUG_TIMER

#include "config.h"

#ifdef CANTATA_DBUG_TIMER
#include <QtCore/QTime>
#include <QtCore/QThread>
#include <QtCore/QDebug>
#define TF_DEBUG DebugTimer xxxx(__PRETTY_FUNCTION__);

struct DebugTimer
{
    DebugTimer(const char *n) : name(n) { time.start(); qWarning() << QThread::currentThreadId() << name << "ENTER"; }
    ~DebugTimer() { qWarning() << QThread::currentThreadId() << name << "LEAVE" << time.elapsed(); }
    QString name;
    QTime time;
};
#else
#define TF_DEBUG
#endif

#endif
