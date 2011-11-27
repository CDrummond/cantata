#ifndef LIBRARYPAGE_H_
#define LIBRARYPAGE_H_

#include "ui_librarypage.h"

class LibraryPage : public QWidget, public Ui::LibraryPage
{
public:
    LibraryPage(QWidget *p) : QWidget(p) { setupUi(this); }
};

#endif
