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
#ifndef FORMATGPX_H
#define FORMATGPX_H

#include <movescount/deviceinfo.h>
#include <movescount/movescount.h>
#include <fstream>
#include <QDomDocument>

class FormatGpx
{
    public:
        FormatGpx(LogEntry* logEntry);
        ~FormatGpx(){};

        void build();
        void setCreator(QString creator);
        void setIncludeOnlyManualLaps(bool onlyManualLaps);
        void addLap(QDomElement* tagLaps, QString lapIndex, QString startLat, QString startLon, QString stopLat, QString stopLon, QString dateTime, QString distance, QString elapsedTime);
        QString getGpx();
        bool writeToFile(QString outFile);
        QString getLastError();

    private:
        QDomDocument mDoc;
        LogEntry* mLogEntry;
        QString mCreator;
        bool mIncludeOnlyManualLaps;
        QString mLastError;
};

#endif
