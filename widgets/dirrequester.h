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
#include "lineedit.h"
class DirRequester : public QWidget
{
    Q_OBJECT
public:
    DirRequester(QWidget *parent);
    virtual ~DirRequester() { }

    QString text() const { return edit->text(); }
    void setText(const QString &t) { edit->setText(t); }

private Q_SLOTS:
    void chooseDir();

private:
    LineEdit *edit;
};
#endif

#endif
