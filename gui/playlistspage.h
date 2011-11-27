#ifndef PLAYLISTSPAGE_H_
#define PLAYLISTSPAGE_H_

#include "ui_playlistspage.h"

class PlaylistsPage : public QWidget, public Ui::PlaylistsPage
{
public:
    PlaylistsPage(QWidget *p) : QWidget(p) { setupUi(this); }
};

#endif
