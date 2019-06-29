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

#include "socialnetworkstrava.h"
#include <QHttpMultiPart>
#include <QDateTime>


SocialNetworkStrava::SocialNetworkStrava(QObject *parent)
    : SocialNetworks(parent, 
            STRAVA,
            "https://www.strava.com/oauth/mobile/authorize",
            "https://www.strava.com/oauth/token",
            "https://www.strava.com/oauth/deauthorize",
            "activity:write,activity:read,profile:write",
            "",
            "http://localhost:8080",
            "",
            "code",
            "force",
            "authorization_code",
            ""),
    mUploadMoves(true),
    mUseGPX(false),
    mUseTCX(true),
    mUseStravaElevation(false),
    mNetMgr(0),
    mUploadTimer(0),
    mUploadedActivityId("")
{
    qDebug() << "strava network initialized";
    mUploadTimer = new QTimer(this);
    
    settings.beginReadArray("socialNetworksSettings");
    settings.setArrayIndex(STRAVA);
    mEnabled = settings.value("enable", false).toBool();
    mAuthUrl = settings.value("authUrl", mAuthUrl).toString();
    mTokenUrl = settings.value("tokenUrl", mTokenUrl).toString();
    mDeAuthUrl = settings.value("deauthorizeUrl", mDeAuthUrl).toString();
    mRedirectUri = settings.value("redirectUrl", mRedirectUri).toString();
    mClientId = settings.value("clientId", "").toString();
    mClientSecret = settings.value("clientSecret", "").toString();
    mScope = settings.value("scope", mScope).toString();
    
    setAuthorizationUrl(QUrl(mAuthUrl));
    setAccessTokenUrl(QUrl(mTokenUrl));
    setScope(mScope);
    setClientIdentifier(mClientId);
    setClientIdentifierSharedKey(mClientSecret);
    
    setAuthStatus((AuthStatus)settings.value("auth_status", DISCONNECTED).toInt());
    setProperty("redirect_uri", mRedirectUri);
    setProperty("response_type", mResponseType);
    setProperty("aproval_prompt", mAprovalPrompt);
    setProperty("grant_type", mGrantType);
    
    auto replyHandler = new QOAuthHttpServerReplyHandler(parent);
    replyHandler->setCallbackText("Authorized, you can close this page.");
    setReplyHandler(replyHandler);

    connect(replyHandler, &QAbstractOAuthReplyHandler::replyDataReceived, this, [=](const QByteArray &data){
            //QJsonParseError err;
            //QJsonDocument json = QJsonDocument::fromJson(data, &err);
            //tokenReceivedCallback(json, "refresh_token");
            });
    
    connect(this, SIGNAL(tokenExpired()), this, SLOT(tokenExpiredCallback()));
    connect(this, SIGNAL(tokenReceived(QJsonDocument, QString)), this, SLOT(tokenReceivedCallback(QJsonDocument, QString)));
    connect(this, &QOAuth2AuthorizationCodeFlow::authorizeWithBrowser, &QDesktopServices::openUrl);
    connect(this, &QOAuth2AuthorizationCodeFlow::authorizationCallbackReceived, [=](QVariantMap data) {
            authCallbackReceived(data);
    });
    connect(this, &QOAuth2AuthorizationCodeFlow::tokenChanged, [=](QString token) {
            setToken(token);
            });
    connect(this, &QOAuth2AuthorizationCodeFlow::finished, [=](QNetworkReply *reply) {
    });
    connect(this, &QOAuth2AuthorizationCodeFlow::requestFailed, [=](const QAbstractOAuth::Error error) {
    });
    connect(this, &QOAuth2AuthorizationCodeFlow::error, [=](const QString &error, const QString &errorDescription, const QUrl &uri) {
            qCritical() << "server responded with error(): " << error;
            qCritical() << "  >> " << errorDescription;
            qCritical() << "  >> " << uri.toString();
//            emit error(QString("Strava server error: %1\nmessage: %2\nURL: %3")
//                   .arg(error)
//                    );
    });
    
    if (mEnabled){
        QDir sDir(QString(getenv("HOME")) + "/.openambit/");
        sDir.mkpath("strava");
    }

    if (mEnabled && getAuthStatus() > DISCONNECTED){
        if (settings.value("access_token", "").toString() == ""){
                settings.endArray();
                disconnect();
                return;
        }
        setUserAgent("openambit (https://github.com/openambitproject/openambit)");

        setToken(settings.value("access_token", "").toString());
        setRefreshToken(settings.value("refresh_token", "").toString());
        setProperty("expires_at", settings.value("expires_at", -1.0).toDouble());
        setProperty("expires_in", settings.value("expires_in", 6*60*60).toDouble());
        setProperty("code", settings.value("code", ""));
        mCode = settings.value("code", "").toString();

        mExpireTime = 1000 * settings.value("expires_at", 6*60*60).toDouble();
        setProperty("expiration", QDateTime::fromMSecsSinceEpoch(mExpireTime));
        if (!isExpired()){
            setStatus("connected");
            settings.setValue("auth_status", CONNECTED);
        
            monitorTokenExpiration(settings.value("expires_in", 6*60*60).toDouble());
        }
        else{
            setStatus("expired");
            settings.setValue("auth_status", EXPIRED);
            settings.endArray();

            if (mExpireTime > 0){
                requestToken(QVariantMap(), "refresh_token");
            }
        }
    }
    else{
        stopListening();
    }

    settings.endArray();
    // configured, call grant() to start the auth flow
}

SocialNetworkStrava::~SocialNetworkStrava()
{
    if (mNetMgr){
        delete mNetMgr;
    }
    if (mUploadTimer){
        delete mUploadTimer;
    }
}

void SocialNetworkStrava::tokenExpiredCallback()
{
    requestToken(QVariantMap(), "refresh_token");
}

void SocialNetworkStrava::tokenReceivedCallback(QJsonDocument json, QString grant_type)
{
    setAuthorized(json, grant_type);
}

/** 
 * callback when the user grants the strava app authorization.
 * http://developers.strava.com/docs/authentication/
 *
 * If a gateway2strava is used (i.e.: a web service which talks to strava api),
 * after all the authorization steps finish (auth+get token) on the server side,
 * redirect the user's browser to localhost with this format:
 * http://localhost:8080/?json_data=<BASE64>&code=<CODE>&error=<IF_ANY>
 * json_data: base64 string of the final json auth data ({"access_token: "..", etc})
 *
 * code: code received in the first auth step.
 * state: state sent when grant() was called
 * error: if any error ocurred (like rate limiting exceded, etc)
 *
 * @param data map of the received data passed back to us as url parameters (code=&access_token=...)
 */
void SocialNetworkStrava::authCallbackReceived(QVariantMap data)
{
    if (data.size() == 0){
        emit error("Authorization error, no token received.");
        return;
    }
    mCode = data["code"].toString();
    setProperty("code", mCode);

    // if we've received the json_data parameter,
    // the auth process has finished (auth + token request)
    if (data["json_data"].toString() != ""){
        if (data["error"].toString() != ""){
            return;
        }

        setAuthorized(QJsonDocument::fromJson(QByteArray::fromBase64(data["json_data"].toByteArray())), "");
        return;
    }

    /* 2nd step, normal flow. Get the access token */
    requestToken(data);
}

bool SocialNetworkStrava::isEnabled()
{
    return mEnabled;
}

void SocialNetworkStrava::setUploadMoves(bool uploadMoves)
{
    mUploadMoves = uploadMoves;
    saveSetting("uploadMoves", mUploadMoves);
}

bool SocialNetworkStrava::getUploadMoves()
{
    return mUploadMoves && mEnabled && getAuthStatus() > DISCONNECTED;
}

SocialNetworks::AuthStatus SocialNetworkStrava::getAuthStatus()
{
    return SocialNetworks::getAuthStatus(STRAVA);
}

QString SocialNetworkStrava::getUploadFormat()
{
    return (mUseGPX) ? "gpx" : "tcx";
}

void SocialNetworkStrava::saveSetting(QString sKey, QVariant sValue)
{
    settings.beginWriteArray("socialNetworksSettings");
    settings.setArrayIndex(STRAVA);
    settings.setValue(sKey, sValue);
    settings.endArray();
}

void SocialNetworkStrava::reloadSettings()
{
    settings.beginReadArray("socialNetworksSettings");
    settings.setArrayIndex(STRAVA);
    mAuthUrl = settings.value("authUrl", "https://www.strava.com/oauth/mobile/authorize").toString();
    mTokenUrl = settings.value("tokenUrl", "https://www.strava.com/oauth/token").toString();
    mDeAuthUrl = settings.value("deauthorizeUrl", "https://www.strava.com/oauth/deauthorize").toString();
    mRedirectUri = settings.value("redirectUrl", "http://localhost:8080").toString();
    mUploadMoves = settings.value("uploadMoves", true).toBool();
    mEnabled = settings.value("enable", true).toBool();
    mUseGPX = settings.value("useGPX", true).toBool();
    mUseTCX = settings.value("useTCX", true).toBool();
    mUseStravaElevation = settings.value("useStravaElevation", false).toBool();
    settings.endArray();
    
    setAuthorizationUrl(QUrl(mAuthUrl));
    setAccessTokenUrl(QUrl(mTokenUrl));
}

void SocialNetworkStrava::disconnect()
{
    QVariantMap parms;
    parms["access_token"] = token();
    stopRefreshToken();

    if (parms["access_token"] == ""){
        clearAuthSettings(STRAVA);
        emit error("No access token saved, I can not disconnect you from Strava servers");
        return;
    }

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

        clearAuthSettings(STRAVA);
    
        mExpireTime = 0;

        QOAuth2AuthorizationCodeFlow::setStatus(QAbstractOAuth::Status::NotAuthenticated);
        emit disconnected();
    });
}

/**
 * Perform a custom post in multipart/data format
 *
 */
QNetworkReply* SocialNetworkStrava::postMultiPart(QString moveUrl, QVariantMap multiPartData)
{
    if (isExpired()){
        requestToken(QVariantMap(), "refresh_token", true);
    }
    if (!mNetMgr){
        mNetMgr = new QNetworkAccessManager(this);
    }

    QHttpMultiPart multiPart;
    QNetworkRequest req;
    req.setUrl(QUrl(moveUrl));
    req.setRawHeader("Authorization", "Bearer " + token().toLatin1());
    req.setRawHeader("accept", "application/json");
    
    QByteArray boundary(multiPart.boundary());
    req.setHeader(QNetworkRequest::ContentTypeHeader,"multipart/form-data; boundary=" + boundary);

    QByteArray data;
    data.insert(0, "--" + boundary + "\r\n");
    for(QVariantMap::const_iterator iter = multiPartData.begin(); iter != multiPartData.end(); ++iter) {
        data.append("Content-Disposition: form-data; name=\"" + iter.key() + "\"\r\n");
        if (iter.key().contains("filename")){
            data.append("Content-Type: application/gpx+xml\r\n\r\n");
        }
        else{
            data.append("\r\n");
        }
        data.append(iter.value().toByteArray());
        data.append("\r\n--" + boundary);
        data.append("\r\n");
    }

    data.replace(data.size()-2, 6, "--\r\n\r\n");
    return mNetMgr->post(req, data);
}


/**
 * Save the authorization data received
 */
void SocialNetworkStrava::setAuthorized(QJsonDocument jsonData, QString grant_type)
{
    if (!jsonData["errors"].isUndefined()){
        emit error(jsonData["message"].toString());
        return;
    }
    setAuthStatus(CONNECTED);

    setToken(jsonData["access_token"].toString());
    setRefreshToken(jsonData["refresh_token"].toString());
    mExpireTime = 1000 * jsonData["expires_at"].toDouble();

    // XXX: saveToken()
    settings.beginWriteArray("socialNetworksSettings");
    settings.setArrayIndex(STRAVA);
    settings.setValue("net_name", "Strava");
    settings.setValue("auth_status", getAuthStatus());

    // only when refreshing a token
    settings.setValue("access_token", jsonData["access_token"].toString());
    settings.setValue("refresh_token", jsonData["refresh_token"].toString());
    settings.setValue("expires_at", jsonData["expires_at"].toDouble());
    settings.setValue("expires_in", jsonData["expires_in"].toDouble());
    settings.setValue("code", mCode);

    if (grant_type == "refresh_token"){
        emit tokenRefreshed(jsonData);
    }
    else{
        // only when requesting a new token
        settings.setValue("firstname", jsonData["athlete"]["firstname"].toString());
        settings.setValue("lastname", jsonData["athlete"]["lastname"].toString());
        settings.setValue("username", jsonData["athlete"]["username"].toString());
        settings.setValue("userid", jsonData["athlete"]["id"].toString());

        emit authorized(jsonData);
    }
    settings.endArray();
    
    setStatus("connected");
    
    monitorTokenExpiration(jsonData["expires_in"].toDouble());
}

/**
 * Only the weight can be updated (api v3)
 * http://developers.strava.com/docs/reference/#api-Athletes-updateLoggedInAthlete
 */ 
void SocialNetworkStrava::updateAthleteData()
{
    settings.beginGroup("athleteSettings");
    double athleteWeight = settings.value("weight", -1.0).toDouble();
    settings.endGroup();

    if (athleteWeight == -1.0){
        emit error("no athlete weight saved yet");
        return;
    }
    QByteArray weight = QString("weight=%1").arg(athleteWeight/100).toUtf8();
    auto reply = put(QUrl("https://www.strava.com/api/v3/athlete"), weight);
    connect(reply, &QNetworkReply::finished, [=]() {
            QByteArray response = reply->readAll();
            reply->deleteLater();

            if (reply->error() != QNetworkReply::NoError){
                qCritical() << "updateAthleteData error: " << reply->errorString();
                emit error("Update athlete data: " + reply->errorString());
                return;
            }
            QJsonParseError err;
            QJsonDocument json = QJsonDocument::fromJson(response, &err);
            
            settings.beginWriteArray("socialNetworksSettings");
            settings.setArrayIndex(STRAVA);
            settings.setValue("firstname", json["firstname"].toString());
            settings.setValue("lastname", json["lastname"].toString());
            settings.setValue("username", json["username"].toString());
            settings.setValue("userid", json["id"].toString());
            settings.endArray();
    });

}

/**
 * http://developers.strava.com/docs/uploads/
 * http://developers.strava.com/docs/reference/#api-Activities-getActivityById
 */
void SocialNetworkStrava::downloadActivity()
{
    auto reply = get(QUrl("https://www.strava.com/api/v3/activities/2352533809"));
    connect(reply, &QNetworkReply::finished, [=]() {
        QByteArray response = reply->readAll();
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError){
            qCritical() << "downloadActivity error: " << reply->errorString();
            qCritical() << response;
            emit (reply->errorString());
            return;
        }
        QJsonParseError err;
        QJsonDocument json = QJsonDocument::fromJson(response, &err);
    });
}

/**
 * 
 * http://developers.strava.com/docs/reference/#api-Uploads-createUpload
 * https://developers.strava.com/playground/#/Uploads/createUpload
 */
void SocialNetworkStrava::uploadActivity(LogEntry* logEntry)
{
    if (!mUploadMoves){
        qDebug() << "Strava: Upload moves disabled";
        return;
    }

    mUploadedActivityId = "";
    QDateTime logTime = logEntry->time;
    QString moveName = QString::fromUtf8(logEntry->logEntry->header.activity_name);
    QString deviceName = logEntry->deviceInfo.name + " " + logEntry->deviceInfo.model;
    QString gpsData;
    QString gpsType = "gpx";

    if (mUseTCX){
        FormatTcx fTcx(logEntry);
        gpsType = "tcx";
        fTcx.build();
        gpsData = fTcx.getTcx();
    }
    else if (mUseGPX){
        FormatGpx fGpx(logEntry);
        fGpx.build();
        gpsData = fGpx.getGpx();
    }
    settings.beginReadArray("socialNetworksSettings");
    settings.setArrayIndex(STRAVA);
    QString activityTitle = settings.value("activityTitle", "{ACTIVITY_NAME}").toString();
    QString activityDescription = settings.value("activityDescription", "").toString();
    int activityPrivate = settings.value("uploadMovesPrivate", 0).toInt();
    bool useStravaElevation = settings.value("useStravaElevation", false).toBool();
    settings.endArray();

    if (useStravaElevation){
        gpsData = gpsData.replace(deviceName, "openambit");
    }

    activityTitle.replace("{ACTIVITY_NAME}", moveName);
    activityDescription.replace("{ACTIVITY_NAME}", moveName);

    QVariantMap multiPartData;
    if (activityTitle != ""){
        multiPartData["name"] = activityTitle;
    }

    if (activityDescription != ""){
        multiPartData["description"] = activityDescription;
    }
    multiPartData["data_type"] = gpsType;
    multiPartData["private"] = activityPrivate;
    multiPartData["file\"; filename=\"ride." + gpsType] = gpsData;
    //multipartData["activity_type"] = data_type;

    auto reply = postMultiPart("https://www.strava.com/api/v3/uploads", multiPartData);
    //auto reply = postMultiPart("http://localhost", multiPartData);
    // XXX: 
    // the default Content-Type sent by post() is application/x-www-form-urlencoded
    // instead of multipart/form-data, even if QHttpMultiPart::FormDataType is used.
    // https://doc.qt.io/qt-5/qabstractoauth.html#ContentType-enum
    connect(reply, &QNetworkReply::uploadProgress, [=](qint64 bytesSent, qint64 bytesTotal) {
            emit moveUploadProgress(bytesSent, bytesTotal);
            }
           );
    connect(reply, &QNetworkReply::finished, [=]() {
        QByteArray response = reply->readAll();
        QJsonParseError err;
        QJsonDocument json = QJsonDocument::fromJson(response, &err);
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError){
            QString str_err(reply->errorString());
            qCritical() << "postMultiPart error: " << str_err;
            qCritical() << response;
            if (!json["error"].isUndefined()){
                str_err += "\n\n" + json["error"].toString();
            }
            emit error(str_err);
            mUploadedActivityId = "";
            return;
        }
        if (mUploadTimer->isActive()){
            mUploadTimer->stop();
        }
        mUploadTimer->disconnect(SIGNAL(timeout()));
        mUploadedActivityId = QString::number(json["id"].toDouble(), 'g', 10);
        connect(mUploadTimer, &QTimer::timeout, this, [=](){
                checkUploadStatus(logTime);
                });
        mUploadTimer->start(5000);
    });
}

/**
 * Posting a file for upload will enqueue it for processing.
 * A successful upload will return a response with an upload ID. You may use this
 * ID to poll the status of your upload.
 * http://developers.strava.com/docs/uploads/
 */
void SocialNetworkStrava::checkUploadStatus(QDateTime dateTime)
{
    if (mUploadedActivityId == ""){
        mUploadTimer->disconnect(SIGNAL(timeout()));
        mUploadTimer->stop();
        return;
    }

    auto reply = get(QUrl("https://www.strava.com/api/v3/uploads/" + mUploadedActivityId));
    connect(reply, &QNetworkReply::finished, [=]() {
        QByteArray response = reply->readAll();
        QJsonParseError err;
        QJsonDocument json = QJsonDocument::fromJson(response, &err);
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError){
            qCritical() << "checkUploadStatus error: " << reply->errorString();
            qCritical() << response;
            emit error(reply->errorString());
            mUploadTimer->disconnect(SIGNAL(timeout()));
            mUploadTimer->stop();
            return;
        }

        if(!json["errors"].isUndefined()){
            emit error(json["status"].toString());
            mUploadTimer->disconnect(SIGNAL(timeout()));
            mUploadTimer->stop();
            return;
        }
        else if(!json["error"].isUndefined() && !json["error"].isNull()){
            return;
        }
        else if(!json["activity_id"].isUndefined() && json["activity_id"].isNull()){
            return;
        }
        emit moveUploaded(dateTime);

        mUploadTimer->disconnect(SIGNAL(timeout()));
        mUploadTimer->stop();
    });
    
}

