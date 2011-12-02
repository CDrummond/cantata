#include "interfacesettings.h"
#include "settings.h"
#include <QtGui/QComboBox>

InterfaceSettings::InterfaceSettings(QWidget *p)
    : QWidget(p)
{
    setupUi(this);
};

void InterfaceSettings::load()
{
    systemTrayCheckBox->setChecked(Settings::self()->useSystemTray());
    systemTrayPopup->setChecked(Settings::self()->showPopups());
    coverArtSize->setCurrentIndex(Settings::self()->coverSize());
}

void InterfaceSettings::save()
{
    Settings::self()->saveUseSystemTray(systemTrayCheckBox->isChecked());
    Settings::self()->saveShowPopups(systemTrayPopup->isChecked());
    Settings::self()->saveCoverSize(coverArtSize->currentIndex());
}
