#include "proxysettings.h"
#include "networkproxyfactory.h"
#include <QtCore/QSettings>

ProxySettings::ProxySettings(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);
}

ProxySettings::~ProxySettings()
{
}

void ProxySettings::load()
{
    QSettings s;
    s.beginGroup(NetworkProxyFactory::kSettingsGroup);

    int mode=s.value("mode", NetworkProxyFactory::Mode_System).toInt();
    proxySystem->setChecked(NetworkProxyFactory::Mode_System==mode);
    proxyDirect->setChecked(NetworkProxyFactory::Mode_Direct==mode);
    proxyManual->setChecked(NetworkProxyFactory::Mode_Manual==mode);
    proxyType->setCurrentIndex(QNetworkProxy::HttpProxy==s.value("type", QNetworkProxy::HttpProxy).toInt() ? 0 : 1);
    proxyHost->setText(s.value("hostname").toString());
    proxyPort->setValue(s.value("port", 8080).toInt());
    proxyAuth->setChecked(s.value("use_authentication", false).toBool());
    proxyUsername->setText(s.value("username").toString());
    proxyPassword->setText(s.value("password").toString());
    s.endGroup();
}

void ProxySettings::save()
{
    QSettings s;
    s.beginGroup(NetworkProxyFactory::kSettingsGroup);

    s.setValue("mode", proxySystem->isChecked()
                            ? NetworkProxyFactory::Mode_System
                            : proxyDirect->isChecked()
                                ? NetworkProxyFactory::Mode_Direct
                                : NetworkProxyFactory::Mode_Manual);
    s.setValue("type", 0==proxyType->currentIndex() ? QNetworkProxy::HttpProxy : QNetworkProxy::Socks5Proxy);
    s.setValue("hostname", proxyHost->text());
    s.setValue("port", proxyPort->value());
    s.setValue("use_authentication", proxyAuth->isChecked());
    s.setValue("username", proxyUsername->text());
    s.setValue("password", proxyPassword->text());
    s.endGroup();
    NetworkProxyFactory::Instance()->ReloadSettings();
}
