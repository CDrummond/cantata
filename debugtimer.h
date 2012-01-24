#ifndef DEBUG_TIMER
#define DEBUG_TIMER

#define USE_DBUG_TIMER

#ifdef USE_DBUG_TIMER
#include <QtCore/QTime>
#include <QtCore/QDebug>
#define TF_DEBUG DebugTimer xxxx(__PRETTY_FUNCTION__);

struct DebugTimer
{
    DebugTimer(const char *n) : name(n) { time.start(); }
    ~DebugTimer() { qWarning() << name << time.elapsed(); }
    QString name;
    QTime time;
};
#else
#define TF_DEBUG
#endif

#endif
