#ifndef SQUEEZEDTEXTLABEL_H
#define SQUEEZEDTEXTLABEL_H

#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KSqueezedTextLabel>
#define SqueezedTextLabel KSqueezedTextLabel
#else
#include <QtGui/QLabel>
class SqueezedTextLabel : public QLabel
{
    public:

    SqueezedTextLabel(QWidget *p) : QLabel(p) { }
    void setTextElideMode(Qt::TextElideMode) { }
};
#endif

#endif // SQUEEZEDTEXTLABEL_H
