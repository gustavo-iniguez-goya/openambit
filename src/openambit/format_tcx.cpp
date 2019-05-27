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

#include "format_tcx.h"

FormatTcx::FormatTcx(LogEntry* logEntry)
    : 
        mLogEntry(0),
        mIncludeOnlyManualLaps(true),
        mLastError("")
{
    mLogEntry = logEntry;
    mCreator = mLogEntry->deviceInfo.name + " " + mLogEntry->deviceInfo.model;
}

void FormatTcx::build()
{
    ambit_log_sample_t *sample=0;
    ambit_log_sample_periodic_value_t *value=0;
    u_int32_t index = 0;
    u_int32_t lapIndex = 0;

    double gps_latitude = -1.0;
    double gps_longitude = -1.0;
    double ele = -1.0;
    bool   isEmptyLap = false;
    double trackDistance = 0; /* total distance of the track, in mts */
    double lastLapDistance = 0;
    double lapDuration = 0;
    QDateTime lastLapTime;
    
    QDomElement root = mDoc.createElement("TrainingCenterDatabase");
    root.setAttribute("xmlns:xsd", "http://www.w3.org/2001/XMLSchema");
    root.setAttribute("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
    root.setAttribute("xmlns",     "http://www.garmin.com/xmlschemas/TrainingCenterDatabase/v2");
    mDoc.appendChild(root);
 
    QDomElement tagActivities = mDoc.createElement("Activities");
    QDomElement tag, subTag, tpxTag;
    QDomText tagText;
    root.appendChild(tag);
    
    mTagActivity = mDoc.createElement("Activity");
    mTagActivity.setAttribute("Sport", mLogEntry->logEntry->header.activity_name);
    tagActivities.appendChild(mTagActivity);

    tag = mDoc.createElement("Creator");
    tag.setAttribute("xsi:type", "Device_t");
    mTagActivity.appendChild(tag);

    subTag = mDoc.createElement("Name");
    tag.appendChild(subTag);
    tagText = mDoc.createTextNode(mLogEntry->deviceInfo.name + " " + mLogEntry->deviceInfo.model);
    subTag.appendChild(tagText);

    subTag = mDoc.createElement("ProductId");
    tag.appendChild(subTag);
    tagText = mDoc.createTextNode("0");
    subTag.appendChild(tagText);

    subTag = mDoc.createElement("UnitId");
    tag.appendChild(subTag);
    tagText = mDoc.createTextNode("0");
    subTag.appendChild(tagText);

    root.appendChild(tagActivities);

    QDateTime dateTime(QDate(mLogEntry->logEntry->header.date_time.year,
                             mLogEntry->logEntry->header.date_time.month,
                             mLogEntry->logEntry->header.date_time.day),
                       QTime(mLogEntry->logEntry->header.date_time.hour,
                             mLogEntry->logEntry->header.date_time.minute, 0).addMSecs(mLogEntry->logEntry->header.date_time.msec));
    dateTime.setTimeSpec(Qt::UTC);
    
    subTag = mDoc.createElement("Id");
    tagText = mDoc.createTextNode(dateTime.toString(Qt::ISODate));
    subTag.appendChild(tagText);
    mTagActivity.appendChild(subTag);

    QDomElement lap = getEmptyLap(dateTime);
    QDomElement lastLap;
    QDomElement trackPoint;
    
    mTagActivity.appendChild(lap);

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

            lastLapDistance = trackDistance;

            if (sample->u.lapinfo.event_type == 0x00){
            }
            else if (sample->u.lapinfo.event_type == 0x1f && !mIncludeOnlyManualLaps){
                lap = newLap(dateTime, QString::number(sample->u.lapinfo.duration/1000), QString::number(sample->u.lapinfo.distance));
                mTagActivity.appendChild(lap);
            }
            else if (sample->u.lapinfo.event_type == 0x1e && !mIncludeOnlyManualLaps){
                lap = newLap(dateTime, QString::number(sample->u.lapinfo.duration/1000), QString::number(sample->u.lapinfo.distance));
                mTagActivity.appendChild(lap);
            }
            else if (sample->u.lapinfo.event_type == 0x01){
                isEmptyLap = (sample->u.lapinfo.distance == 0 && sample->u.lapinfo.duration == 0);
                
                lap = mDoc.createElement("Lap");
                lap.setAttribute("StartTime", dateTime.toString(Qt::ISODate));
                mTagActivity.appendChild(lap);
    
                if (!isEmptyLap){
                    lastLapTime = dateTime;
                    lastLap = mTagActivity.elementsByTagName("Lap").at(mTagActivity.elementsByTagName("Lap").length()-2).toElement();
                    QDomNode node = lastLap.elementsByTagName("TotalTimeSeconds").at(0);
                    QDomNode nodeTrack = lastLap.elementsByTagName("Track").at(0);
                    if (!lastLap.isNull()){
                        lastLap.removeChild(node);
                        tag = mDoc.createElement("TotalTimeSeconds");
                        tagText = mDoc.createTextNode(QString::number(sample->u.lapinfo.duration/1000));
                        tag.appendChild(tagText);
                        
                        if (!nodeTrack.isNull()){
                            lastLap.insertBefore(tag, nodeTrack);
                        }
                        else{
                            lastLap.appendChild(tag);
                        }
                        
                        node = lastLap.elementsByTagName("DistanceMeters").at(0);
                        lastLap.removeChild(node);
                        tag = mDoc.createElement("DistanceMeters");
                        tagText = mDoc.createTextNode(QString::number(sample->u.lapinfo.distance));
                        tag.appendChild(tagText);

                        if (nodeTrack.isNull()){
                            lastLap.appendChild(tag);
                        }
                        else{
                            lastLap.insertBefore(tag, nodeTrack);
                        }
                
                        tag = mDoc.createElement("TotalTimeSeconds");
                        lap.appendChild(tag);
                        tagText = mDoc.createTextNode("0");
                        tag.appendChild(tagText);

                        tag = mDoc.createElement("DistanceMeters");
                        lap.appendChild(tag);
                        tagText = mDoc.createTextNode("0");
                        tag.appendChild(tagText);
                        
                        mTrack = mDoc.createElement("Track");
                        lap.appendChild(mTrack);
                    }
                    lapIndex++;
                }
                else{
                    tag = mDoc.createElement("TotalTimeSeconds");
                    lap.appendChild(tag);
                    tagText = mDoc.createTextNode(QString::number(sample->u.lapinfo.duration/1000));
                    tag.appendChild(tagText);
                    
                    tag = mDoc.createElement("DistanceMeters");
                    lap.appendChild(tag);
                    tagText = mDoc.createTextNode(QString::number(sample->u.lapinfo.distance));
                    tag.appendChild(tagText);
                    
                }

                tag = mDoc.createElement("Intensity");
                lap.appendChild(tag);
                tagText = mDoc.createTextNode("Active"); // resting
                tag.appendChild(tagText);

                tag = mDoc.createElement("TriggerMethod");
                lap.appendChild(tag);
                tagText = mDoc.createTextNode("Manual"); // distance, location, time, heartrate
                tag.appendChild(tagText);
                
                if (lap.elementsByTagName("Track").at(0).isNull()){
                    mTrack = mDoc.createElement("Track");
                    lap.appendChild(mTrack);
                }

                if (!isEmptyLap){
                    lastLap = lap;
                }
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
        }
        else if (sample->type == ambit_log_sample_type_periodic){
            if (gps_latitude == -1.0){
                index++;
                continue;
            }
            dateTime.setDate(QDate(sample->utc_time.year, sample->utc_time.month, sample->utc_time.day));
            dateTime.setTime(QTime(sample->utc_time.hour, sample->utc_time.minute, 0).addMSecs(sample->utc_time.msec));
            dateTime.setTimeSpec(Qt::UTC);

            trackPoint = mDoc.createElement("Trackpoint");
            mTrack.appendChild(trackPoint);

            tag = mDoc.createElement("Time");
            tagText = mDoc.createTextNode(dateTime.toString(Qt::ISODate));
            tag.appendChild(tagText);
            trackPoint.appendChild(tag);
            
            tag = mDoc.createElement("Position");
            trackPoint.appendChild(tag);
            subTag = mDoc.createElement("LatitudeDegrees");
            tagText = mDoc.createTextNode(QString::number(gps_latitude, 'f', 8));
            subTag.appendChild(tagText);
            tag.appendChild(subTag);
            
            subTag = mDoc.createElement("LongitudeDegrees");
            tagText = mDoc.createTextNode(QString::number(gps_longitude, 'f', 8));
            subTag.appendChild(tagText);
            tag.appendChild(subTag);

            tag = mDoc.createElement("Extensions");
            tpxTag = mDoc.createElement("TPX");
            tpxTag.setAttribute("xmlns", "http://www.garmin.com/xmlschemas/ActivityExtension/v2");
            tag.appendChild(tpxTag);
            trackPoint.appendChild(tag);

            for (int i=0; i < sample->u.periodic.value_count; i++) {
                value = &sample->u.periodic.values[i];
                switch(value->type) {
                    case ambit_log_sample_periodic_type_altitude:
                        if (value->u.altitude >= -1000 && value->u.altitude <= 15000) {
                            ele = value->u.altitude;
                        }
                        break;
                    case ambit_log_sample_periodic_type_hr:
                        tag = mDoc.createElement("HeartRateBpm");
                        subTag = mDoc.createElement("Value");
                        tag.appendChild(subTag);
                        if (value->u.hr != 0xff && value->u.hr < 300){
                            tagText = mDoc.createTextNode(QString::number((double)value->u.hr));
                        }
                        else{
                            tagText = mDoc.createTextNode("0");
                        }
                        subTag.appendChild(tagText);
                        trackPoint.appendChild(tag);
                        break;
                    case ambit_log_sample_periodic_type_cadence:
                        tag = mDoc.createElement("Cadence");
                        if (value->u.cadence != 0xff && value->u.cadence < 1000) {
                            tagText = mDoc.createTextNode(QString("%1").arg(value->u.cadence));
                        }
                        else{
                            tagText = mDoc.createTextNode("0");
                        }
                        tag.appendChild(tagText);
                        trackPoint.appendChild(tag);
                        break;
                    case ambit_log_sample_periodic_type_bikepower:
                        tag = mDoc.createElement("Watts");
                        if (value->u.bikepower != 0xffff && value->u.bikepower <= 2000) {
                            tagText = mDoc.createTextNode(QString("%1").arg(value->u.bikepower));
                        }
                        else{
                            tagText = mDoc.createTextNode("0");
                        }
                        tag.appendChild(tagText);
                        tpxTag.appendChild(tag);
                        break;
                    case ambit_log_sample_periodic_type_wristcadence:
                        // TODO check units
                        tag = mDoc.createElement("RunCadence");
                        if (value->u.wristaccspeed != 0xffff) {
                            tagText = mDoc.createTextNode(QString("%1").arg(value->u.wristcadence));
                        }
                        else{
                            tagText = mDoc.createTextNode("0");
                        }
                        tag.appendChild(tagText);
                        tpxTag.appendChild(tag);
                        break;
                    // not supported on this format
                    case ambit_log_sample_periodic_type_temperature:
                        tag = mDoc.createElement("Temperature");
                        tagText = mDoc.createTextNode(QString::number((int)value->u.temperature/10));
                        tag.appendChild(tagText);
                        trackPoint.appendChild(tag);
                        break;
                    case ambit_log_sample_periodic_type_speed:
                        tag = mDoc.createElement("Speed");
                        if (0xffff != value->u.speed){
                            tagText = mDoc.createTextNode(QString::number(value->u.speed*0.01, 'f', 4));
                        }
                        else{
                            tagText = mDoc.createTextNode("0");
                        }
                        tag.appendChild(tagText);
                        tpxTag.appendChild(tag);
                        break;
                    case ambit_log_sample_periodic_type_distance:
                        if (value->u.distance != 0xffffffff && value->u.distance != 0xb400000) {
                            trackDistance = value->u.distance;
                        }
                        break;
                    case ambit_log_sample_periodic_type_time:
                        lapDuration += value->u.time/1000;
                        break;
                    default:
                        break;
                }
            }
            tag = mDoc.createElement("AltitudeMeters");
            tagText = mDoc.createTextNode(QString::number(ele));
            tag.appendChild(tagText);
            trackPoint.appendChild(tag);
        }

        index++;
    }

    lastLap = mTagActivity.elementsByTagName("Lap").at(mTagActivity.elementsByTagName("Lap").length()-1).toElement();
    QDomNode node = lastLap.elementsByTagName("DistanceMeters").at(0);
    lastLap.removeChild(node);
    tag = mDoc.createElement("DistanceMeters");
    tagText = mDoc.createTextNode(QString::number(trackDistance-lastLapDistance));
    tag.appendChild(tagText);
    lastLap.appendChild(tag);
    
    node = lastLap.elementsByTagName("TotalTimeSeconds").at(0);
    lastLap.removeChild(node);
    tag = mDoc.createElement("TotalTimeSeconds");
    tagText = mDoc.createTextNode(QString::number(lastLapTime.msecsTo(dateTime)/1000));
    tag.appendChild(tagText);
    lastLap.appendChild(tag);

}

QDomElement FormatTcx::getEmptyLap(QDateTime dateTime)
{
    QDomElement lap = mDoc.createElement("Lap");
    lap.setAttribute("StartTime", dateTime.toString(Qt::ISODate));
    mTagActivity.appendChild(lap);
    
    QDomElement tag = mDoc.createElement("TotalTimeSeconds");
    lap.appendChild(tag);
    QDomText tagText = mDoc.createTextNode("0");
    tag.appendChild(tagText);
    
    tag = mDoc.createElement("DistanceMeters");
    lap.appendChild(tag);
    tagText = mDoc.createTextNode("0");
    tag.appendChild(tagText);
                
    mTrack = mDoc.createElement("Track");
    lap.appendChild(mTrack);
    
    return lap;
}

QDomElement FormatTcx::newLap(QDateTime dateTime, QString seconds, QString distance)
{
    QDomElement lap = mDoc.createElement("Lap");
    lap.setAttribute("StartTime", dateTime.toString(Qt::ISODate));
    mTagActivity.appendChild(lap);
    
    QDomElement tag = mDoc.createElement("TotalTimeSeconds");
    lap.appendChild(tag);
    QDomText tagText = mDoc.createTextNode(seconds);
    tag.appendChild(tagText);
    
    tag = mDoc.createElement("DistanceMeters");
    lap.appendChild(tag);
    tagText = mDoc.createTextNode(distance);
    tag.appendChild(tagText);
                
    mTrack = mDoc.createElement("Track");
    lap.appendChild(mTrack);
    
    return lap;
}

void FormatTcx::insertEmptyLap(int beforePos, QDateTime dateTime)
{

    QDomElement newlap = getEmptyLap(dateTime);
    newlap.setAttribute("StartTime", dateTime.toString(Qt::ISODate));

    QDomNode lap = mTagActivity.elementsByTagName("Lap").at(beforePos);
    if (!lap.isNull()){
        mTagActivity.insertBefore(newlap, lap);
    }
}

void FormatTcx::setCreator(QString creator)
{
    mCreator = creator;
}

void FormatTcx::setIncludeOnlyManualLaps(bool onlyManualLaps)
{
    mIncludeOnlyManualLaps = onlyManualLaps;
}

QString FormatTcx::getTcx()
{
    return "<?xml version=\"1.0\"?>\n" + mDoc.toString(2);
}

bool FormatTcx::writeToFile(QString outFile)
{
    try{
        std::ofstream tcxFile;
        tcxFile.open(outFile.toStdString());
        tcxFile << getTcx().toStdString();
        tcxFile.close();

        return true;
    }
    catch(std::ios_base::failure& e){
        qDebug() << "FormatTcx::writeToFile() error: " << e.what();
        mLastError = e.what();
    }
    return false;
}

QString FormatTcx::getLastError()
{
    return mLastError;
}
