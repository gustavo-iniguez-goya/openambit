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
#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include <QDebug>
#include <iostream>
#include <fstream>
#include <QMessageBox>

SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);

    setupStrava();

    readSettings();

    showHideUserSettings();

    connect(ui->listSettingGroups, SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)), this, SLOT(changePage(QListWidgetItem*,QListWidgetItem*)));
    connect(ui->checkBoxMovescountEnable, SIGNAL(clicked()), this, SLOT(showHideUserSettings()));
    connect(ui->checkBoxStravaEnable, SIGNAL(clicked()), this, SLOT(enableStravaSettings()));

}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

void SettingsDialog::enableStravaSettings()
{
    bool isChecked = ui->checkBoxStravaEnable->isChecked();
    ui->groupBoxStravaSettings->setEnabled(isChecked);
    settings.beginWriteArray("socialNetworksSettings");
    settings.setArrayIndex(STRAVA);
    settings.setValue("enable", isChecked);
    settings.endArray();
}

void SettingsDialog::cmdStravaAuthorized()
{
    if ((ui->lineEditStravaAuthUrl->text().indexOf("https://www.strava.com") != -1 &&
            ui->lineEditStravaClientId->text() != "" &&
            ui->lineEditStravaClientSecret->text() != "") ||
            (ui->lineEditStravaClientId->text() == "" && ui->lineEditStravaClientSecret->text() == "" &&
             ui->lineEditStravaAuthUrl->text().indexOf("https://www.strava.com") == -1)){
        if (ui->lineEditStravaAuthUrl->text().indexOf("https://www.strava.com") != -1){
            strava->setClientIdentifier(ui->lineEditStravaClientId->text());
            strava->setClientIdentifierSharedKey(ui->lineEditStravaClientSecret->text());
        }
        strava->grant();
    }
    else{
        qCritical() << "If you use https://www.strava.com directly,\nthe client id and client secret can not be empty.";
        QMessageBox::warning(nullptr, "Warning", QObject::tr("If you use https://www.strava.com directly,\nthe client id and client secret can not be empty."), QMessageBox::Ok);
    }
}

void SettingsDialog::cmdStravaRefresh()
{
    ui->lblStatus->setText("Refreshing, wait...");
    strava->requestToken(QVariantMap(), "refresh_token");
}

void SettingsDialog::cmdStravaDisconnect()
{
    ui->lblStatus->setText("Disconnecting, wait...");
    strava->disconnect();
}

void SettingsDialog::cmdStravaSettings()
{
    ui->stravaPages->setCurrentIndex(1);
}

void SettingsDialog::cmdStravaOptions()
{
    ui->stravaPages->setCurrentIndex(0);
}

void SettingsDialog::changePage(QListWidgetItem *current, QListWidgetItem *previous)
{
    if (!current)
        current = previous;

    ui->stackedWidget->setCurrentIndex(ui->listSettingGroups->row(current));
}

void SettingsDialog::update()
{
    writeSettings();
}

void SettingsDialog::accept()
{
    writeSettings();
    this->close();
}

void SettingsDialog::showHideUserSettings()
{
    if (ui->checkBoxMovescountEnable->isChecked()) {
        ui->lineEditEmail->setHidden(false);
    }
    else {
        ui->lineEditEmail->setHidden(true);
    }
}

void SettingsDialog::readSettings()
{
    settings.beginGroup("generalSettings");
    ui->checkBoxSkipBetaCheck->setChecked(settings.value("skipBetaCheck", false).toBool());
    ui->checkBoxRunningBackground->setChecked(settings.value("runningBackground", true).toBool());
    settings.endGroup();

    settings.beginGroup("syncSettings");
    ui->checkBoxSyncAutomatically->setChecked(settings.value("syncAutomatically", false).toBool());
    ui->checkBoxSyncTime->setChecked(settings.value("syncTime", true).toBool());
    ui->checkBoxSyncOrbit->setChecked(settings.value("syncOrbit", true).toBool());
    ui->checkBoxSyncSportsMode->setChecked(settings.value("syncSportMode", false).toBool());
    ui->checkBoxSyncNavigation->setChecked(settings.value("syncNavigation", false).toBool());
    ui->checkBoxSyncServerMoves->setChecked(settings.value("syncServerMoves", false).toBool());
    settings.endGroup();

    settings.beginGroup("movescountSettings");
    ui->checkBoxNewVersions->setChecked(settings.value("checkNewVersions", true).toBool());
    ui->checkBoxMovescountEnable->setChecked(settings.value("movescountEnable", false).toBool());
    ui->lineEditEmail->setText(settings.value("email", "").toString());
    ui->checkBoxDebugFiles->setChecked(settings.value("storeDebugFiles", true).toBool());
    settings.endGroup();

    settings.beginReadArray("socialNetworksSettings");
    settings.setArrayIndex(STRAVA);
    bool stravaEnabled = settings.value("enable", false).toBool();
    ui->checkBoxStravaEnable->setChecked(stravaEnabled);
    ui->groupBoxStravaSettings->setEnabled(settings.value("enable", false).toBool());
    ui->lineEditStravaClientId->setText(settings.value("clientId", "").toString());
    ui->lineEditStravaClientSecret->setText(settings.value("clientSecret", "").toString());

    if (stravaEnabled && strava->getAuthStatus() > SocialNetworks::DISCONNECTED){
        ui->lineEditStravaAuthUrl->setText(settings.value("authUrl", strava->getAuthUrl()).toString());
        ui->lineEditStravaTokenUrl->setText(settings.value("tokenUrl", strava->getTokenUrl()).toString());
        ui->lineEditStravaDeauthorizeUrl->setText(settings.value("deauthorizeUrl", strava->getDeauthUrl()).toString());
        ui->lineEditStravaRedirectUrl->setText(settings.value("redirectUrl", strava->getRedirectUrl()).toString());

        ui->lineEditStravaActivityTitle->setText(settings.value("activityTitle", "{ACTIVITY_NAME}").toString());
        ui->lineEditStravaActivityDescription->setText(settings.value("activityDescription", "").toString());
        ui->checkBoxUploadMoves->setChecked(settings.value("uploadMoves", true).toBool());
        ui->checkBoxStravaPrivate->setChecked(settings.value("uploadMovesPrivate", false).toBool());
        ui->checkBoxStravaUseElevation->setChecked(settings.value("useStravaElevation", false).toBool());
        ui->radioButtonStravaGPX->setChecked(settings.value("useGPX", true).toBool());
        ui->radioButtonStravaTCX->setChecked(settings.value("useTCX", false).toBool());
        QString athletename = settings.value("firstname", "<firstname>").toString();
        QString athletelastname = settings.value("lastname", "<lastname>").toString();
        // array must be closed here, before set athlete data
        settings.endArray();
        setStravaAthleteInformation(
                QString("<h4>%1 %2</h4>")
                .arg(athletename)
                .arg(athletelastname)
            );
        setStravaProfilePicture();
    }
    else{
        settings.endArray();
    }
}

void SettingsDialog::writeSettings()
{
    settings.beginGroup("generalSettings");
    settings.setValue("skipBetaCheck", ui->checkBoxSkipBetaCheck->isChecked());
    settings.setValue("runningBackground", ui->checkBoxRunningBackground->isChecked());
    settings.endGroup();

    settings.beginGroup("syncSettings");
    settings.setValue("syncAutomatically", ui->checkBoxSyncAutomatically->isChecked());
    settings.setValue("syncTime", ui->checkBoxSyncTime->isChecked());
    settings.setValue("syncOrbit", ui->checkBoxSyncOrbit->isChecked());
    settings.setValue("syncSportMode", ui->checkBoxSyncSportsMode->isChecked());
    settings.setValue("syncNavigation", ui->checkBoxSyncNavigation->isChecked());
    settings.setValue("syncServerMoves", ui->checkBoxSyncServerMoves->isChecked());
    settings.endGroup();

    settings.beginGroup("movescountSettings");
    settings.setValue("checkNewVersions", ui->checkBoxNewVersions->isChecked());
    settings.setValue("movescountEnable", ui->checkBoxMovescountEnable->isChecked());
    if (ui->checkBoxMovescountEnable->isChecked()) {
        settings.setValue("email", ui->lineEditEmail->text());
    }
    else {
        settings.setValue("email", "");
    }
    settings.setValue("storeDebugFiles", ui->checkBoxDebugFiles->isChecked());
    settings.endGroup();

    settings.beginWriteArray("socialNetworksSettings");
    settings.setArrayIndex(STRAVA);
    settings.setValue("enable", ui->checkBoxStravaEnable->isChecked());
    if (ui->checkBoxStravaEnable->isChecked()) {
        settings.setValue("clientId", ui->lineEditStravaClientId->text());
        settings.setValue("clientSecret", ui->lineEditStravaClientSecret->text());
        settings.setValue("authUrl", ui->lineEditStravaAuthUrl->text());
        settings.setValue("tokenUrl", ui->lineEditStravaTokenUrl->text());
        settings.setValue("redirectUrl", ui->lineEditStravaRedirectUrl->text());
        settings.setValue("deauthorizeUrl", ui->lineEditStravaDeauthorizeUrl->text());
        //settings.setValue("scope", ui->lineEditStravaScope->text());
        settings.setValue("activityTitle", ui->lineEditStravaActivityTitle->text());
        settings.setValue("activityDescription", ui->lineEditStravaActivityDescription->text());
        settings.setValue("uploadMoves", ui->checkBoxUploadMoves->isChecked());
        settings.setValue("uploadMovesPrivate", ui->checkBoxStravaPrivate->isChecked());
        settings.setValue("useGPX", ui->radioButtonStravaGPX->isChecked());
        settings.setValue("useTCX", ui->radioButtonStravaTCX->isChecked());
        settings.setValue("useStravaElevation", ui->checkBoxStravaUseElevation->isChecked());
    }
    settings.endArray();

    emit settingsSaved();
}


void SettingsDialog::setupStrava()
{
    strava = (SocialNetworkStrava*)SocialNetworks::instance(this)->getNetwork(NetworkType::STRAVA);
    if (!strava){
        return;
    }
    connect(ui->cmdStravaAuthorize, SIGNAL(released()), this, SLOT(cmdStravaAuthorized()) );
    connect(ui->cmdStravaRefresh, SIGNAL(released()), this, SLOT(cmdStravaRefresh()) );
    connect(ui->cmdStravaDisconnect, SIGNAL(released()), this, SLOT(cmdStravaDisconnect()) );
    connect(ui->cmdStravaSettings, SIGNAL(released()), this, SLOT(cmdStravaSettings()) );
    connect(ui->cmdStravaOptions, SIGNAL(released()), this, SLOT(cmdStravaOptions()) );

    connect(strava, &SocialNetworkStrava::error, [=](QString error) {
            ui->lblStatus->setText(QString("Error: %1").arg(error));
    });
    connect(strava, &SocialNetworkStrava::tokenRefreshed, [=](QJsonDocument data) {
            setStravaConnected();
    });
    connect(strava, &SocialNetworkStrava::disconnected, [=]() {
            setStravaDisconnected();
        });
    connect(strava, &SocialNetworkStrava::authorized, [=](QJsonDocument data) {
        setStravaConnected();

        auto reply = strava->get(data["athlete"]["profile"].toString());
        connect(reply, &QNetworkReply::finished, [=]() {
            if (reply->error() != QNetworkReply::NoError){
                qCritical() << reply->errorString();
                return;
            }
            std::ofstream userPic;
            userPic.open(QString("%1%2").arg(getenv("HOME")).arg("/.openambit/strava/profile.jpg").toStdString());
            userPic << reply->readAll().toStdString();
            userPic.close();

            setStravaProfilePicture();

            reply->deleteLater();
        });

        setStravaAthleteInformation(
                QString("<h4>%1 %2</h4>")
                    .arg(data["athlete"]["firstname"].toString())
                    .arg(data["athlete"]["lastname"].toString())
                );
    });

    if (strava->isEnabled()){
        if (strava->getAuthStatus() == SocialNetworkStrava::EXPIRED){
            ui->lblStatus->setText("<b>Connected (expired)</b>");
            ui->cmdStravaRefresh->setVisible(true);
            ui->cmdStravaRefresh->setEnabled(true);
            ui->cmdStravaAuthorize->setVisible(false);
            ui->cmdStravaDisconnect->setVisible(false);
            ui->cmdStravaSettings->setEnabled(true);
        }
        else if (strava->getAuthStatus() == SocialNetworkStrava::CONNECTED){
            setStravaConnected();
        }
        else{
            setStravaDisconnected();
        }
    }
    else{
        setStravaDisconnected();
    }
}

void SettingsDialog::setStravaConnected()
{
    ui->lblStatus->setText("<b>Connected</b>");
    ui->cmdStravaRefresh->setVisible(false);
    ui->cmdStravaAuthorize->setVisible(false);
    ui->cmdStravaDisconnect->setVisible(true);
    ui->cmdStravaDisconnect->setEnabled(true);
}

void SettingsDialog::setStravaDisconnected()
{
    ui->lblStatus->setText("<b>Not connected</b>");
    ui->lblProfileImg->setText("");
    ui->lblProfileImg->setPixmap(QPixmap());
    ui->lblProfileImg->setVisible(false);
    setStravaAthleteInformation("");
    ui->cmdStravaRefresh->setVisible(false);
    ui->cmdStravaAuthorize->setVisible(true);
    ui->cmdStravaDisconnect->setVisible(false);
}

void SettingsDialog::setStravaAthleteInformation(QString athleteData)
{
    settings.beginGroup("athleteSettings");
    int weight = settings.value("weight", 0).toInt();
    int restHR = settings.value("restHR", 0).toInt();
    int maxHR  = settings.value("maxHR",  0).toInt();
    int fitL   = settings.value("fitnessLevel", 0).toInt();
    settings.endGroup();

    if (weight != 0 && athleteData != ""){
        athleteData += QString("<br><b>Weight:</b> %1 Kgs.").arg((double)weight*0.01);
    }
    if (restHR != 0 && athleteData != ""){
        athleteData += QString("<br><b>Rest HR:</b> %1 bpm - <b>Max HR:</b> %2 bpm - <b>fitness level:</b> %3").arg(restHR).arg(maxHR).arg(fitL);
    }

    ui->lblAthleteName->setText(athleteData);
}

void SettingsDialog::setStravaProfilePicture()
{
    QPixmap aProfileImg(
                QString("%1%2")
                    .arg(getenv("HOME"))
                    .arg("/.openambit/strava/profile.jpg")
                ,
                "JPEG");
    ui->lblProfileImg->setPixmap(
            aProfileImg.scaled(
                ui->lblProfileImg->width(),
                ui->lblProfileImg->height(),
                Qt::IgnoreAspectRatio)
            );
    ui->lblProfileImg->setVisible(true);
}
