/*
 * (C) Copyright 2013 Emil Ljungdahl
 *
 * This file is part of Openambit.
 *
 * Openambit is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Contributors:
 *
 */
#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QDebug>
#include <QtNetwork>
#include <QtNetworkAuth>
#include <QDesktopServices>
#include <QJsonDocument>
#include "settings.h"
#include "socialnetworks/socialnetworkstrava.h"

class QListWidgetItem;

namespace Ui {
class SettingsDialog;
}

class SettingsDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit SettingsDialog(QWidget *parent = 0);
    ~SettingsDialog();

signals:
    void settingsSaved();

public slots:
    void cmdStravaAuthorized();
    void cmdStravaRefresh();
    void cmdStravaDisconnect();
    void cmdStravaOptions();
    void cmdStravaSettings();
    void enableStravaSettings();
    void changePage(QListWidgetItem *current, QListWidgetItem *previous);
    void accept();
    void update();
    void showHideUserSettings();
    
private:
    void readSettings();
    void writeSettings();

    void setupStrava();
    void setStravaProfilePicture();
    void setStravaConnected();
    void setStravaDisconnected();
    void setStravaAthleteInformation(QString athleteName);

    Ui::SettingsDialog *ui;
    Settings settings;
    SocialNetworkStrava *strava;
};

#endif // SETTINGSDIALOG_H
