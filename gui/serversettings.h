#ifndef SERVERSETTINGS_H
#define SERVERSETTINGS_H

#include "ui_serversettings.h"

class ServerSettings : public QWidget, private Ui::ServerSettings
{
public:
    ServerSettings(QWidget *p);
    virtual ~ServerSettings() { }

    void load();
    void save();
};

#endif
