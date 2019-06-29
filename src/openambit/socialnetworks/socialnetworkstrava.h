/*
 * (C) Copyright 2019 Gustavo Iñiguez Goya
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
#ifndef SOCIALNETWORKSTRAVA_H
#define SOCIALNETWORKSTRAVA_H

#include "socialnetworks.h"
#include <vector>
#include <QDebug>
#include <QtNetwork>
#include <QtNetworkAuth>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QTimer>
    

/**
 * doc: http://developers.strava.com/docs/
 */
class SocialNetworkStrava : public SocialNetworks
{
    Q_OBJECT
    
public:
    SocialNetworkStrava(QObject *parent);
    ~SocialNetworkStrava();

    void disconnect();
    QNetworkReply* postMultiPart(QString moveUrl, QVariantMap multiPartData);
    void refreshStravaToken(bool waitToFinish=false);
    void setAuthorized(QJsonDocument jsonData, QString grant_type);
    void updateAthleteData();
    void downloadActivity();
    void uploadActivity(LogEntry* logEntry);

    bool isEnabled();
    void setUploadMoves(bool uploadMoves);
    bool getUploadMoves();
    void saveSetting(QString sKey, QVariant sValue);
    void reloadSettings();
    QString getUploadFormat();
    SocialNetworks::AuthStatus getAuthStatus();

public slots:
    void authCallbackReceived(QVariantMap data);
    void tokenExpiredCallback();
    void tokenReceivedCallback(QJsonDocument json, QString grant_type);
    void checkUploadStatus(QDateTime dateTime);

signals:
    void error(QString err);
    void authorized(QJsonDocument data);
    void tokenRefreshed(QJsonDocument data);
    void disconnected();
    void moveUploadProgress(qint64 bytesSent, qint64 bytesTotal);
    void moveUploaded(QDateTime moveDate);

private:
    bool mUploadMoves;
    bool mUseGPX;
    bool mUseTCX;
    bool mUseStravaElevation;
    QString mToken;
    QNetworkAccessManager *mNetMgr;
    QTimer *mUploadTimer;
    QString mUploadedActivityId;
};

#endif // SOCIALNETWORKSTRAVA_H
