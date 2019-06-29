/*
 * (C) Copyright 2019 Gustavo Iñiguez Goia
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

#include "socialnetworks.h"
#include "socialnetworkstrava.h"
#include <QHttpMultiPart>
#include <QDateTime>

// a global object which will hold all the 3rd party api services
static SocialNetworks *gNetworksInstance;

SocialNetworks::SocialNetworks(QObject *parent, 
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
        QString code)
    : QOAuth2AuthorizationCodeFlow(parent),
    mEnabled(false),
    mRefreshTimer(0),
    mAuthStatus(DISCONNECTED),
    mStatus("not connected")
{
    mRefreshTimer = new QTimer(this);

    for (int i=0;i < NetworkType::TOTAL_NETS;i++){
        mNetworks.push_back(0);
    }

    mAuthUrl       = authUrl;
    mTokenUrl      = tokenUrl;
    mDeAuthUrl     = deAuthUrl;
    mScope         = scope;
    mClientId      = clientId;
    mRedirectUri   = redirectUri;
    mClientSecret  = clientSecret;
    mResponseType  = responseType;
    mAprovalPrompt = aprovalPrompt;
    mCode          = code;
    
    connect(this, &QOAuth2AuthorizationCodeFlow::finished, [=]() {
            stopListening();
    });
}

SocialNetworks::~SocialNetworks()
{
}

SocialNetworks::SocialNetworks(QObject *parent) 
    : QOAuth2AuthorizationCodeFlow(parent)
{
    for (int i=0;i < NetworkType::TOTAL_NETS;i++){
        mNetworks.push_back(0);
    }
}

void SocialNetworks::listen()
{
    QOAuthHttpServerReplyHandler* httpServer = (QOAuthHttpServerReplyHandler *)replyHandler();
    if (httpServer)
        httpServer->listen(QHostAddress("127.0.0.1"), 8080);
}

void SocialNetworks::stopListening()
{
    QOAuthHttpServerReplyHandler* httpServer = (QOAuthHttpServerReplyHandler *)replyHandler();
    if (httpServer)
        httpServer->close();
}

SocialNetworks* SocialNetworks::getNetwork(NetworkType net)
{
    return mNetworks[net];
}

void SocialNetworks::grant()
{
    setState(QString("openambit%1").arg(QDateTime::currentMSecsSinceEpoch()));
    listen();
    QOAuth2AuthorizationCodeFlow::grant();
}

void SocialNetworks::disconnect()
{
    QVariantMap parms;
    parms["access_token"] = token();
    
    auto reply = post(QUrl(mDeAuthUrl), parms);
    connect(reply, &QNetworkReply::finished, [=]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError){
            qCritical() << "Deauth error: " << reply->errorString();
            emit error(reply->errorString());
            return;
        }

        setToken("");
        // the scope is lost once we disconnect. We need to set it again
        setScope(mScope);

        mExpireTime = 0;

        QOAuth2AuthorizationCodeFlow::setStatus(QAbstractOAuth::Status::NotAuthenticated);
        stopListening();
        emit disconnected();
        });
}

void SocialNetworks::setAuthStatus(AuthStatus status)
{
    mAuthStatus = status;
}

SocialNetworks::AuthStatus SocialNetworks::getAuthStatus(NetworkType net)
{
    return mAuthStatus;
}

void SocialNetworks::clearAuthSettings(NetworkType net)
{
    settings.beginWriteArray("socialNetworksSettings");
    settings.setArrayIndex(net);
    settings.remove("");
    settings.setValue("auth_status", DISCONNECTED);
    settings.endArray();
}

QString SocialNetworks::getAuthUrl()
{
    return mAuthUrl;
}

QString SocialNetworks::getTokenUrl()
{
    return mTokenUrl;
}

QString SocialNetworks::getDeauthUrl()
{
    return mDeAuthUrl;
}

QString SocialNetworks::getRedirectUrl()
{
    return mRedirectUri;
}

SocialNetworks* SocialNetworks::instance(QObject *parent)
{
    if (!gNetworksInstance){
        gNetworksInstance = new SocialNetworks(parent);
    }

    return gNetworksInstance;
}

void SocialNetworks::setStatus(QString status)
{
    mStatus = status;
}

QString SocialNetworks::getStatus()
{
    return mStatus;
}

void SocialNetworks::initialize(QObject *parent, int netType)
{
    if (netType == STRAVA){
        SocialNetworks *ghostNet = mNetworks[STRAVA];
        if (!ghostNet){
            mNetworks[STRAVA] = new SocialNetworkStrava(parent);
        }
    }
    else{
        qDebug() << "sNets::initialize() Unknown net " << netType << " : " << STRAVA;
    }
}

/**
 * Monitor token expiration in the following cases:
 * - openambit starts up and the access token has not expired.
 * - an access token is refreshed.
 */
void SocialNetworks::monitorTokenExpiration(double expiresIn)
{
    if (expiresIn*1000 <= 0){
        return;
    }
    stopRefreshToken();
    mRefreshTimer->disconnect(SIGNAL(timeout()));
    connect(mRefreshTimer, SIGNAL(timeout()), this, SLOT(tokenExpiredCallback()));
    mRefreshTimer->start((long)expiresIn*1000);
}

void SocialNetworks::tokenExpiredCallback()
{
    emit tokenExpired();
}

void SocialNetworks::stopRefreshToken()
{
    if (mRefreshTimer){
        mRefreshTimer->stop();
    }
}

/**
 * Refresh or request a new token.
 *
 * @param grant_type: "refresh_token" or "authorization_code"
 * @param waitForReply: refresh the token synchronously
 */
void SocialNetworks::requestToken(QVariantMap data, QString grant_type, bool waitForReply)
{
    QOAuth2AuthorizationCodeFlow::setStatus(QAbstractOAuth::Status::RefreshingToken);

    QVariantMap parms;
    parms["grant_type"] = grant_type;
    parms["client_id"] = mClientId;
    parms["client_secret"] = mClientSecret;
    if (grant_type == "refresh_token"){
        setStatus("refreshing token");

        parms["refresh_token"] = refreshToken();
        if (parms["refresh_token"] == ""){
            QOAuth2AuthorizationCodeFlow::setStatus(QAbstractOAuth::Status::NotAuthenticated);
            emit error("invalid refresh_token, it's empty. Refresh it manually or grant access again");
            return;
        }
    }
    else{
        setStatus("authenticating");
        parms["code"] = data["code"];
    }
    QNetworkReply* reply=NULL;
    if (isExpired()){
         reply = QOAuth2AuthorizationCodeFlow::post(QUrl(mTokenUrl), parms);
    }
    else{
         reply = post(QUrl(mTokenUrl), parms);
    }
    
    if (waitForReply){
        QEventLoop loop;
        connect(reply, &QNetworkReply::finished, [=](){
            requestTokenCallback(reply, grant_type);
                });
        connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
        loop.exec();
        return;
    }

    connect(reply, &QNetworkReply::finished, [=](){
            requestTokenCallback(reply, grant_type);
    });
}

void SocialNetworks::requestTokenCallback(QNetworkReply* reply, QString grant_type)
{
        QByteArray response = reply->readAll();
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError){
            qCritical() << "requestToken error: " << reply->errorString();
            qCritical() << response;
            if (grant_type == "refresh_token"){
                monitorTokenExpiration(60 * 10);
            }
            QOAuth2AuthorizationCodeFlow::setStatus(QAbstractOAuth::Status::NotAuthenticated);
            emit error(reply->errorString());
            return;
        }

        QJsonParseError err;
        QJsonDocument json = QJsonDocument::fromJson(response, &err);

        QOAuth2AuthorizationCodeFlow::setStatus(QAbstractOAuth::Status::Granted);
        stopListening();
        emit tokenReceived(json, grant_type);
}

QNetworkReply* SocialNetworks::post(QUrl url, QVariantMap data)
{
    if (isExpired()){
        requestToken(QVariantMap(), "refresh_token", true);
    }
    return QOAuth2AuthorizationCodeFlow::post(url, data);
}

QNetworkReply* SocialNetworks::put(QUrl url, QByteArray &data)
{
    if (isExpired()){
        requestToken(QVariantMap(), "refresh_token", true);
    }
    return QOAuth2AuthorizationCodeFlow::put(url, data);
}

QNetworkReply* SocialNetworks::get(QUrl url)
{
    if (isExpired()){
        requestToken(QVariantMap(), "refresh_token", true);
    }
    return QOAuth2AuthorizationCodeFlow::get(url);
}
