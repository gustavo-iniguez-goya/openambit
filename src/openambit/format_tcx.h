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
#ifndef FORMATTCX_H
#define FORMATTCX_H

#include <movescount/deviceinfo.h>
#include <movescount/movescount.h>
#include <fstream>
#include <QDomDocument>

class FormatTcx
{
    public:
        FormatTcx(LogEntry* logEntry);
        ~FormatTcx(){};

        void build();
        void setCreator(QString creator);
        void setIncludeOnlyManualLaps(bool onlyManualLaps);
        QDomElement getEmptyLap(QDateTime dateTime);
        QDomElement newLap(QDateTime dateTime, QString seconds, QString distance);
        void insertEmptyLap(int beforePos, QDateTime dateTime);
        QDomElement getLastLap();
        QString getTcx();
        bool writeToFile(QString outFile);
        QString getLastError();

    private:
        QDomDocument mDoc;
        QDomElement mTagActivity;
        QDomElement mTrack;
        LogEntry* mLogEntry;
        QString mCreator;
        bool mIncludeOnlyManualLaps;
        QString mLastError;
};

#endif
