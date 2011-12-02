#include "dirrequester.h"
#include <QtGui/QToolButton>
#include <QtGui/QFileDialog>
#include <QtGui/QHBoxLayout>
#include <QtGui/QIcon>

DirRequester::DirRequester(QWidget *parent)
    : QWidget(parent)
{
    QHBoxLayout *layout=new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    edit=new LineEdit(this);
    QToolButton *button=new QToolButton(this);
    layout->addWidget(edit);
    layout->addWidget(button);
    button->setAutoRaise(true);
    button->setIcon(QIcon::fromTheme("document-open"));
    connect(button, SIGNAL(clicked(bool)), SLOT(chooseDir()));
}

void DirRequester::chooseDir()
{
    QString dir=QFileDialog::getExistingDirectory(this, tr("Select Folder"));
    if (!dir.isEmpty()) {
        edit->setText(dir);
    }
}
