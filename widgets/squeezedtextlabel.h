#ifndef SQUEEZEDTEXTLABEL_H
#define SQUEEZEDTEXTLABEL_H

#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KSqueezedTextLabel>
class SqueezedTextLabel : public KSqueezedTextLabel
{
public:
    SqueezedTextLabel(QWidget *p) : KSqueezedTextLabel(p) { setTextElideMode(Qt::ElideRight); }
};
#else
#include <QtGui/QLabel>
class SqueezedTextLabel : public QLabel
{
public:
    SqueezedTextLabel(QWidget *p) : QLabel(p) { }
};
#endif

#endif // SQUEEZEDTEXTLABEL_H
