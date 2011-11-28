#include "playbacksettings.h"
#include "lib/mpdconnection.h"

PlaybackSettings::PlaybackSettings(QWidget *p)
    : QWidget(p)
{
    setupUi(this);
    replayGain->addItem(i18n("None"), QVariant("off"));
    replayGain->addItem(i18n("Track"), QVariant("track"));
    replayGain->addItem(i18n("Album"), QVariant("album"));
};

void PlaybackSettings::load()
{
    crossfading->setValue(MPDStatus::self()->crossFade());
    QString rg=MPDConnection::self()->getReplayGain();
    replayGain->setCurrentIndex(0);

    for(int i=0; i<replayGain->count(); ++i) {
        if (replayGain->itemData(i).toString()==rg){
            replayGain->setCurrentIndex(i);
            break;
        }
    }
}

void PlaybackSettings::save()
{
    MPDConnection::self()->setCrossFade(crossfading->value());
    MPDConnection::self()->setReplayGain(replayGain->itemData(replayGain->currentIndex()).toString());
}
