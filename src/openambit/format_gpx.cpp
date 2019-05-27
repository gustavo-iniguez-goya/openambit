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

#include "format_gpx.h"

FormatGpx::FormatGpx(LogEntry* logEntry)
    : 
        mLogEntry(0),
        mIncludeOnlyManualLaps(false),
        mLastError("")
{
    mLogEntry = logEntry;
    mCreator = mLogEntry->deviceInfo.name + " " + mLogEntry->deviceInfo.model;
}

void FormatGpx::build()
{
    ambit_log_sample_t *sample=0;
    ambit_log_sample_periodic_value_t *value=0;
    u_int32_t index = 0;
    u_int32_t lapIndex = 0;

    double gps_latitude = -1.0;
    double gps_longitude = -1.0;
    double ele = -1.0;
    bool lapStart = false;
    double trackDistance = 0; /* total distance of the track, in mts */
    double lastLapDistance = 0;
    double totalLapsDistance = 0;
    QDateTime lastLapTime;
    double lapDuration = 0;
    double lapStartLon=-1.0, lapStartLat=-1.0;
    double lapStopLat=-1.0, lapStopLon=-1.0;
    int power=0;
    
    QDomElement root = mDoc.createElement("gpx");
    root.setAttribute("version", "1.1");
    root.setAttribute("creator", mLogEntry->deviceInfo.name + " " + mLogEntry->deviceInfo.model);
    root.setAttribute("xmlns:xsd", "http://www.w3.org/2001/XMLSchema");
    root.setAttribute("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
    root.setAttribute("xmlns",     "http://www.topografix.com/GPX/1/1");
    root.setAttribute("xsi:schemaLocation", "http://www.topografix.com/GPX/1/1 http://www.topografix.com/GPX/1/1/gpx.xsd http://www.garmin.com/xmlschemas/GpxExtensions/v3 http://www.garmin.com/xmlschemas/GpxExtensionsv3.xsd http://www.garmin.com/xmlschemas/TrackPointExtension/v1 http://www.garmin.com/xmlschemas/TrackPointExtensionv1.xsd http://www.cluetrust.com/XML/GPXDATA/1/0 http://www.cluetrust.com/Schemas/gpxdata10.xsd");
    root.setAttribute("xmlns:gpxtx", "http://www.garmin.com/xmlschemas/TrackPointExtension/v1");
    root.setAttribute("xmlns:gpxdata", "http://www.cluetrust.com/XML/GPXDATA/1/0");
    root.setAttribute("xmlns:gpxx", "http://www.garmin.com/xmlschemas/GpxExtensions/v3");
    mDoc.appendChild(root);
 
    QDomElement tag, subTag, tpxText;
    QDomText tagText;
    tag = mDoc.createElement("metadata");
    root.appendChild(tag);
    subTag = mDoc.createElement("time");
    tag.appendChild(subTag);
    
    QDateTime dateTime(QDate(mLogEntry->logEntry->header.date_time.year,
                             mLogEntry->logEntry->header.date_time.month,
                             mLogEntry->logEntry->header.date_time.day),
                       QTime(mLogEntry->logEntry->header.date_time.hour,
                             mLogEntry->logEntry->header.date_time.minute, 0).addMSecs(mLogEntry->logEntry->header.date_time.msec));
    dateTime.setTimeSpec(Qt::UTC);

    tagText = mDoc.createTextNode(dateTime.toString(Qt::ISODate));
    subTag.appendChild(tagText);

    tag = mDoc.createElement("trk");
    root.appendChild(tag);
    subTag = mDoc.createElement("name");
    tagText = mDoc.createTextNode(mLogEntry->logEntry->header.activity_name);
    subTag.appendChild(tagText);
    tag.appendChild(subTag);

    subTag = mDoc.createElement("type");
    tagText = mDoc.createTextNode("1");
    subTag.appendChild(tagText);
    tag.appendChild(subTag);
    
    QDomElement tagTrkseg = mDoc.createElement("trkseg");
    tag.appendChild(tagTrkseg);
    
    QDomElement tagLaps = mDoc.createElement("extensions");
    QDomElement tagExtension = mDoc.createElement("extensions");
    
    while (index < mLogEntry->logEntry->samples_count) {
        sample = &mLogEntry->logEntry->samples[index];

        if (sample->type == ambit_log_sample_type_gps_tiny ||
            sample->type == ambit_log_sample_type_gps_base ||
            sample->type == ambit_log_sample_type_gps_small
                ){
            if (sample->type == ambit_log_sample_type_gps_base){
                // coordinates in degrees
                gps_latitude = (double)sample->u.gps_base.latitude * 0.0000001;
                gps_longitude = (double)sample->u.gps_base.longitude * 0.0000001;

                ele = (double)sample->u.gps_base.altitude*0.01;
                dateTime.setDate(QDate(sample->u.gps_base.utc_base_time.year, sample->u.gps_base.utc_base_time.month, sample->u.gps_base.utc_base_time.day));
                dateTime.setTime(QTime(sample->u.gps_base.utc_base_time.hour, sample->u.gps_base.utc_base_time.minute, 0).addMSecs(sample->u.gps_base.utc_base_time.msec));
            }
            else if (sample->type == ambit_log_sample_type_gps_small || sample->type == ambit_log_sample_type_gps_tiny){
                if (sample->type == ambit_log_sample_type_gps_small){
                    gps_latitude = (double)sample->u.gps_small.latitude * 0.0000001;
                    gps_longitude = (double)sample->u.gps_small.longitude * 0.0000001;
                }
                else if(sample->type == ambit_log_sample_type_gps_tiny){
                    gps_latitude = (double)sample->u.gps_tiny.latitude * 0.0000001;
                    gps_longitude = (double)sample->u.gps_tiny.longitude * 0.0000001;
                }

                dateTime = QDateTime::fromMSecsSinceEpoch(sample->utc_time.msec);
            }
            dateTime.setTimeSpec(Qt::UTC);

            index++;
            continue;
        } 
        else if (sample->type == ambit_log_sample_type_lapinfo){
            dateTime.setDate(QDate(sample->utc_time.year, sample->utc_time.month, sample->utc_time.day));
            dateTime.setTime(QTime(sample->utc_time.hour, sample->utc_time.minute, 0).addMSecs(sample->utc_time.msec));
            dateTime.setTimeSpec(Qt::UTC);

            if (sample->u.lapinfo.event_type != 0x01){
            }
            if (sample->u.lapinfo.event_type == 0x00){
                lapStart = !lapStart;
            }
            else if (sample->u.lapinfo.event_type == 0x1f){
                // distance and time are always 0
                lapStart = true;
                lapStartLat = gps_latitude;
                lapStartLon = gps_longitude;
                lastLapTime = dateTime;
                index++;
                continue;
            }
            else if (sample->u.lapinfo.event_type == 0x1e){
                lapStart = false;
                lapStopLat = lapStartLat;
                lapStopLon = lapStartLon;
                lapStartLat = gps_latitude;
                lapStartLon = gps_longitude;
                lastLapDistance = sample->u.lapinfo.distance;
                index++;
                continue;
            }
            else if (sample->u.lapinfo.event_type == 0x01){
                lapStopLat = lapStartLat;
                lapStopLon = lapStartLon;
                lapStartLat = gps_latitude;
                lapStartLon = gps_longitude;
                lastLapDistance = sample->u.lapinfo.distance;
                lapIndex++;
                lapStart = !lapStart;
            }
            else if (sample->u.lapinfo.event_type == 0x16){
                qDebug() << "lap interval start";
            }
            else if (sample->u.lapinfo.event_type == 0x15){
                qDebug() << "lap low interval end";
            }
            else if (sample->u.lapinfo.event_type == 0x14){
                qDebug() << "lap high interval end";
            }
            
            lapDuration += sample->u.lapinfo.duration/1000;
            totalLapsDistance += sample->u.lapinfo.distance;

            addLap(&tagLaps,
                    QString::number(lapIndex),
                    QString::number(lapStartLat),
                    QString::number(lapStartLon),
                    QString::number(lapStopLat),
                    QString::number(lapStopLon),
                    dateTime.toString(Qt::ISODateWithMs),
                    QString::number(lastLapDistance),
                    QString::number((double)sample->u.lapinfo.duration/1000.0));
        }
        else if (sample->type == ambit_log_sample_type_periodic){
            if (gps_latitude == -1.0){
                index++;
                continue;
            }
            QDomElement tagExtensions = mDoc.createElement("extensions");
            QDomElement tagTrackpoint = mDoc.createElement("gpxtpx:TrackPointExtension");
            tagTrackpoint.setAttribute("xmlns:gpxtpx", "http://www.garmin.com/xmlschemas/TrackPointExtension/v1");

            for (int i=0; i < sample->u.periodic.value_count; i++) {
                value = &sample->u.periodic.values[i];
                switch(value->type) {
                    case ambit_log_sample_periodic_type_longitude:
                        if (value->u.longitude != 0xffffffff && value->u.longitude <= 180 && value->u.longitude >= -180){
                            qDebug() << "LOOOONGITUDE: " << (double)value->u.longitude/10000000;
                        }
                        break;
                    case ambit_log_sample_periodic_type_latitude:
                        if (value->u.latitude != 0xffffffff && value->u.latitude <= 180 && value->u.latitude >= -180){
                            qDebug() << "LATITUDE: " << (double)value->u.latitude/10000000;
                        }
                        break;
                    case ambit_log_sample_periodic_type_energy:
                        if (value->u.energy && value->u.energy <= 1000) {
                            tag = mDoc.createElement("gpxdata:energy");
                            tagText = mDoc.createTextNode(QString::number((double)value->u.energy/10.0));
                            tag.appendChild(tagText);
                            tagExtensions.appendChild(tag);
                        }
                        break;
                    case ambit_log_sample_periodic_type_gpsspeed:
                        if (value->u.gpsspeed != 0xffff) {
                            qDebug() << "GPSSpeed: " << (double)value->u.gpsspeed/100.0;
                        }
                        break;
                    case ambit_log_sample_periodic_type_altitude:
                        if (value->u.altitude >= -1000 && value->u.altitude <= 15000) {
                            ele = value->u.altitude;

                            tag = mDoc.createElement("gpxdata:altitude");
                            tagText = mDoc.createTextNode(QString::number(value->u.altitude));
                            tag.appendChild(tagText);
                            tagExtensions.appendChild(tag);
                        }
                        break;
                    case ambit_log_sample_periodic_type_verticalspeed:
                        if (value->u.altitude >= -1000 && value->u.altitude <= 15000) {
                            tag = mDoc.createElement("gpxdata:verticalSpeed");
                            tagText = mDoc.createTextNode(QString::number((double)value->u.verticalspeed*0.01));
                            tag.appendChild(tagText);
                            tagExtensions.appendChild(tag);
                        }
                        break;
                    case ambit_log_sample_periodic_type_hr:
                        tag = mDoc.createElement("gpxtpx:hr");
                        if (value->u.hr != 0xff && value->u.hr < 300){
                            tagText = mDoc.createTextNode(QString::number((double)value->u.hr));
                        }
                        else{
                            tagText = mDoc.createTextNode("0");
                        }
                        tag.appendChild(tagText);
                        tagTrackpoint.appendChild(tag);
                        break;
                    case ambit_log_sample_periodic_type_cadence:
                        tag = mDoc.createElement("gpxdata:cadence");
                        if (value->u.cadence != 0xff && value->u.cadence < 1000) {
                            tagText = mDoc.createTextNode(QString("%1").arg(value->u.cadence));
                        }
                        else{
                            tagText = mDoc.createTextNode("0");
                        }
                        tag.appendChild(tagText);
                        tagExtensions.appendChild(tag);
                        break;
                    case ambit_log_sample_periodic_type_bikepower:
                        /* strava only recognizes <power> */
                        if (value->u.bikepower != 0xffff && value->u.bikepower <= 2000) {
                            tag = mDoc.createElement("gpxdata:power");
                            tagText = mDoc.createTextNode(QString("%1").arg(value->u.bikepower));
                            tag.appendChild(tagText);
                            tagExtensions.appendChild(tag);
                        }
                        break;
                    case ambit_log_sample_periodic_type_wristcadence:
                        // TODO check units
                        if (value->u.wristaccspeed != 0xffff) {
                        }
                        else{
                        }
                        break;
                    case ambit_log_sample_periodic_type_temperature:
                        tag = mDoc.createElement("gpxdata:temp");
                        tagText = mDoc.createTextNode(QString::number((int)value->u.temperature/10));
                        tag.appendChild(tagText);
                        tagExtensions.appendChild(tag);
                        break;
                    case ambit_log_sample_periodic_type_sealevelpressure:
                        tag = mDoc.createElement("gpxdata:seaLevelPressure");
                        tagText = mDoc.createTextNode(QString("%1").arg(value->u.sealevelpressure/10));
                        tag.appendChild(tagText);
                        tagExtensions.appendChild(tag);
                        break;
                    case ambit_log_sample_periodic_type_speed:
                        tag = mDoc.createElement("gpxdata:speed");
                        if (0xffff != value->u.speed){
                            tagText = mDoc.createTextNode(QString::number(value->u.speed*0.01, 'f', 4));
                        }
                        else{
                            tagText = mDoc.createTextNode("0");
                        }
                        tag.appendChild(tagText);
                        tagExtensions.appendChild(tag);
                        break;
                    case ambit_log_sample_periodic_type_distance:
                        tag = mDoc.createElement("gpxdata:distance");
                        if (value->u.distance != 0xffffffff && value->u.distance != 0xb400000) {
                            trackDistance = value->u.distance;
                            tagText = mDoc.createTextNode(QString::number(value->u.distance));
                        }
                        else{
                            tagText = mDoc.createTextNode("0");
                        }
                        tag.appendChild(tagText);
                        tagExtensions.appendChild(tag);
                        break;
                    case ambit_log_sample_periodic_type_time:
                        lapDuration += value->u.time/1000;
                        break;
                    default:
                        break;
                }
            }
            dateTime.setDate(QDate(sample->utc_time.year, sample->utc_time.month, sample->utc_time.day));
            dateTime.setTime(QTime(sample->utc_time.hour, sample->utc_time.minute, 0).addMSecs(sample->utc_time.msec));
            dateTime.setTimeSpec(Qt::UTC);

            tag = mDoc.createElement("trkpt");
            tag.setAttribute("lat", QString::number(gps_latitude, 'f', 8));
            tag.setAttribute("lon", QString::number(gps_longitude, 'f', 8));
            tagTrkseg.appendChild(tag);
            subTag = mDoc.createElement("time");
            tagText = mDoc.createTextNode(dateTime.toString(Qt::ISODateWithMs));
            subTag.appendChild(tagText);
            tag.appendChild(subTag);
            subTag = mDoc.createElement("ele");
            tagText = mDoc.createTextNode(QString("%1").arg(QString::number(ele, 'f', 1)));
            subTag.appendChild(tagText);
            tag.appendChild(subTag);
            subTag = mDoc.createElement("power");
            tagText = mDoc.createTextNode(QString("%1").arg(QString::number(power)));
            subTag.appendChild(tagText);
            tag.appendChild(subTag);

            tagExtensions.appendChild(tagTrackpoint);
            tag.appendChild(tagExtensions);

            tagTrkseg.appendChild(tag);
        }
        index++;
    }
    addLap(&tagLaps,
            QString::number(lapIndex+1),
            QString::number(lapStartLat),
            QString::number(lapStartLon),
            QString::number(lapStopLat),
            QString::number(lapStopLon),
            dateTime.toString(Qt::ISODateWithMs),
            QString::number(trackDistance - totalLapsDistance),
            QString::number(lastLapTime.msecsTo(dateTime)/1000));

    root.appendChild(tagLaps);
}

void FormatGpx::setCreator(QString creator)
{
    mCreator = creator;
}

void FormatGpx::setIncludeOnlyManualLaps(bool onlyManualLaps)
{
    mIncludeOnlyManualLaps = onlyManualLaps;
}

void FormatGpx::addLap(QDomElement* tagLaps, QString lapIndex, QString startLat, QString startLon, QString stopLat, QString stopLon, QString dateTime, QString distance, QString elapsedTime)
{
    QDomElement tag = mDoc.createElement("gpxdata:lap");
    QDomElement subTag = mDoc.createElement("gpxdata:index");
    QDomNode tagText = mDoc.createTextNode(lapIndex);
    subTag.appendChild(tagText);
    tag.appendChild(subTag);
    
    subTag = mDoc.createElement("gpxdata:startPoint");
    subTag.setAttribute("lat", startLat);
    subTag.setAttribute("lon", startLon);
    tag.appendChild(subTag);
    
    subTag = mDoc.createElement("gpxdata:endPoint");
    subTag.setAttribute("lat", stopLat);
    subTag.setAttribute("lon", stopLon);
    tag.appendChild(subTag);
    
    subTag = mDoc.createElement("gpxdata:startTime");
    tagText = mDoc.createTextNode(dateTime);
    subTag.appendChild(tagText);
    tag.appendChild(subTag);
    
    subTag = mDoc.createElement("gpxdata:distance");
    tagText = mDoc.createTextNode(distance);
    subTag.appendChild(tagText);
    tag.appendChild(subTag);
    
    subTag = mDoc.createElement("gpxdata:elapsedTime");
    tagText = mDoc.createTextNode(elapsedTime);
    subTag.appendChild(tagText);
    tag.appendChild(subTag);

    tagLaps->appendChild(tag);
}

QString FormatGpx::getGpx()
{
    return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" + mDoc.toString(2);
}

bool FormatGpx::writeToFile(QString outFile)
{
    try{
        std::ofstream gpxFile;
        gpxFile.open(outFile.toStdString());
        gpxFile << getGpx().toStdString();
        gpxFile.close();

        return true;
    }
    catch(std::ios_base::failure& e){
        qDebug() << "FormatGpx::writeToFile() error: " << e.what();
    }
    return false;
}

QString FormatGpx::getLastError()
{
    return mLastError;
}
