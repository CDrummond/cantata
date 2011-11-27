#ifndef LYRICSPAGE_H_
#define LYRICSPAGE_H_

#include "ui_lyricspage.h"

class LyricsPage : public QWidget, public Ui::LyricsPage
{
public:
    LyricsPage(QWidget *p) : QWidget(p) { setupUi(this); }
};

#endif
