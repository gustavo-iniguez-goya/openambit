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
#ifndef SOCIALNETWORKS_H
#define SOCIALNETWORKS_H

#include "settings.h"
#include "format_tcx.h"
#include "format_gpx.h"
#include <vector>
#include <QDebug>
#include <QtNetwork>
#include <QtNetworkAuth>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QTimer>
    
enum NetworkType {
    NONE = -1,
    STRAVA = 0,
    TOTAL_NETS
};


class SocialNetworks : public QOAuth2AuthorizationCodeFlow
{
    Q_OBJECT

public:
    enum AuthStatus {
        DISCONNECTED = -1,
        CONNECTED = 0,
        EXPIRED = 1
    };

    SocialNetworks(QObject *parent, 
            NetworkType netType,
            QString authUrl,
            QString tokenUrl,
            QString deAuthUrl,
            QString scope,
            QString clientId,
            QString redirectUri,
            QString clientSecret,
            QString responseType,
            QString aprovalPrompt,
            QString grantType,
            QString code);
    SocialNetworks(QObject *parent);
    ~SocialNetworks();

    void initialize(QObject *parent, int netType);
    static SocialNetworks* instance(QObject *parent);
    virtual void grant();
    virtual void disconnect();
    virtual void listen();
    virtual void stopListening();
    SocialNetworks* getNetwork(NetworkType _net);

    void setAuthStatus(AuthStatus status);
    SocialNetworks::AuthStatus getAuthStatus(NetworkType net);
    void clearAuthSettings(NetworkType net);
    
    void setStatus(QString status);
    QString getStatus();

    QString getAuthUrl();
    QString getTokenUrl();
    QString getDeauthUrl();
    QString getRedirectUrl();

    virtual bool isExpired(){
        return QDateTime::currentMSecsSinceEpoch() > mExpireTime;
    }
    virtual bool isEnabled(){ return mEnabled; };
    virtual void requestToken(QVariantMap data, QString grant_type="authorization_code", bool waitForReply=false);
    void requestTokenCallback(QNetworkReply* reply, QString grant_type);
    virtual void monitorTokenExpiration(double expiresIn);
    virtual void stopRefreshToken();
    virtual QNetworkReply* post(QUrl url, QVariantMap data);
    virtual QNetworkReply* put(QUrl url, QByteArray &data);
    virtual QNetworkReply* get(QUrl url);

private slots:
    void tokenExpiredCallback();

signals:
    void error(QString err);
    void settingsSaved();
    void tokenExpired();
    void tokenReceived(QJsonDocument json, QString grant_type);
    void disconnected();

protected:
    std::vector<SocialNetworks*> mNetworks;

    Settings settings;
    double mExpireTime=0;
    bool mEnabled;
    QTimer *mRefreshTimer;
    
    QString mAuthUrl;
    QString mTokenUrl;
    QString mDeAuthUrl;
    QString mScope;
    QString mClientId;
    QString mRedirectUri;
    QString mClientSecret;
    QString mResponseType;
    QString mAprovalPrompt;
    QString mGrantType;
    QString mCode;
    AuthStatus mAuthStatus;
    QString mStatus;
};
    
#endif // SOCIALNETWORKS_H
