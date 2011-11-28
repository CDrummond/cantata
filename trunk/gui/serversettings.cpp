#include "serversettings.h"
#include "settings.h"

ServerSettings::ServerSettings(QWidget *p)
    : QWidget(p)
{
    setupUi(this);
};

void ServerSettings::load()
{
    hostLineEdit->setText(Settings::self()->connectionHost());
    portSpinBox->setValue(Settings::self()->connectionPort());
    passwordLineEdit->setText(Settings::self()->connectionPasswd());
    mpdDir->setText(Settings::self()->mpdDir());
}

void ServerSettings::save()
{
    Settings::self()->saveConnectionHost(hostLineEdit->text());
    Settings::self()->saveConnectionPort(portSpinBox->value());
    Settings::self()->saveConnectionPasswd(passwordLineEdit->text());
    Settings::self()->saveMpdDir(mpdDir->text());
}
