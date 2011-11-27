#ifndef FOLDERPAGE_H_
#define FOLDERPAGE_H_

#include "ui_folderpage.h"

class FolderPage : public QWidget, public Ui::FolderPage
{
public:
    FolderPage(QWidget *p) : QWidget(p) { setupUi(this); }
};

#endif
