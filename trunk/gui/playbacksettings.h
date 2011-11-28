#ifndef PLAYBACKSETTINGS_H
#define PLAYBACKSETTINGS_H

#include "ui_playbacksettings.h"

class PlaybackSettings : public QWidget, private Ui::PlaybackSettings
{
public:
    PlaybackSettings(QWidget *p);
    virtual ~PlaybackSettings() { }

    void load();
    void save();
};

#endif
