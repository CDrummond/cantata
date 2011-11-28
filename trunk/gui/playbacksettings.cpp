#include "playbacksettings.h"
#include "lib/mpdconnection.h"

PlaybackSettings::PlaybackSettings(QWidget *p)
    : QWidget(p)
{
    setupUi(this);
};

void PlaybackSettings::load()
{
    crossfading->setValue(MPDStatus::self()->xfade());
}

void PlaybackSettings::save()
{
    MPDConnection::self()->setCrossfade(crossfading->value());
}
