#ifndef DIR_REQUESER_H
#define DIR_REQUESER_H

#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KUrlRequester>
class DirRequester : public KUrlRequester
{
public:
    DirRequester(QWidget *parent) : KUrlRequester(parent) {
        setMode(KFile::Directory|KFile::ExistingOnly|KFile::LocalOnly);
    }
};

#else
#include <QtGui/QLineEdit>
#define DirRequester QLineEdit
#endif

#endif
