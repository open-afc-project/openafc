#!/usr/bin/env python3
#
# Copyright (C) 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

"""Data transformation and database schema update module for FCC ULS and Canadian datasets."""

import csv
import datetime
import fnmatch
import hashlib
import os
from collections import OrderedDict
import sqlalchemy as sa

# File types we need to consider along with their # of | symbols (i.e. # of cols - 1)
neededFilesUS = {}
neededFilesUS[0] = {
    'AN.dat': 37,
    'CP.dat': 13,
    'EM.dat': 15,
    'EN.dat': 29,
    'FR.dat': 29,
    'HD.dat': 58,
    'LO.dat': 50,
    'PA.dat': 21,
    'SG.dat': 14,
}
neededFilesUS[1] = {
    'AN.dat': 37,
    'CP.dat': 13,
    'EM.dat': 15,
    'EN.dat': 29,
    'FR.dat': 29,
    'HD.dat': 58,
    'LO.dat': 50,
    'PA.dat': 23,
    'SG.dat': 14,
}

# Version changed AUG 18, 2022
versionTime = datetime.datetime(2022, 8, 18, 0, 0, 0)

# Map to reuse weekday in loops
dayMap = OrderedDict()
dayMap[6] = 'sun'
dayMap[0] = 'mon'
dayMap[1] = 'tue'
dayMap[2] = 'wed'
dayMap[3] = 'thu'
dayMap[4] = 'fri'
dayMap[5] = 'sat'

# Map to reuse for converting month strings to ints
monthMap = {
    'jan': 1,
    'feb': 2,
    'mar': 3,
    'apr': 4,
    'may': 5,
    'jun': 6,
    'jul': 7,
    'aug': 8,
    'sep': 9,
    'oct': 10,
    'nov': 11,
    'dec': 12,
}


def verifyCountsFile(directory):
    """Return the datetime object based on the counts file."""
    with open(directory + '/counts', 'r') as countsFile:
        line = countsFile.readline()
        # FCC Format: File Creation Date: Sun Oct  3 17:59:04 EDT 2021
        dateStr = line.replace('File Creation Date: ', '')
        dateData = dateStr.split()
        month = monthMap.get(dateData[1].lower(), 'err')
        day = int(dateData[2])
        time = dateData[3]
        year = int(dateData[5])
        timeData = [int(string) for string in time.split(':')]
        hours = timeData[0]
        mins = timeData[1]
        sec = timeData[2]
        if month != 'err':
            fileCreationDate = datetime.datetime(
                year, month, day, hours, mins, sec)
            return fileCreationDate
        else:
            raise Exception(
                'ERROR: Could not parse month of FCC string in counts file for ' +
                directory + ' update')


def removeFromCombinedFile(fileName, directory, ids_to_remove, day, versionIdx):
    """Remove any record with given ids from the combined weekly/daily file."""
    weeklyAndDailyPath = directory + '/weekly/' + fileName + '_withDaily'

    if day == 'weekly':
        with open(weeklyAndDailyPath, 'w', encoding='utf8') as withDaily:
            with open(directory + '/weekly/' + fileName, 'r', encoding='utf8') as weekly:
                record = ''
                symbolCount = 0
                numExpectedCols = neededFilesUS[versionIdx][fileName]
                for line in weekly:
                    line = line.replace('\n', '').replace('\r', '')
                    if line == '' or line == ' ':
                        continue
                    elif '|' not in line:
                        record += line
                        continue
                    else:
                        symbolCount += line.count('|')
                        record += line
                    if symbolCount == numExpectedCols:
                        cols = record.split('|')
                        fileType = cols[0]
                        if fileType + ".dat" in list(neededFilesUS[versionIdx].keys()):
                            if cols[1] not in ids_to_remove:
                                record += "\r\n"
                                withDaily.write(record)
                            record = ''
                            symbolCount = 0
                    elif symbolCount > numExpectedCols:
                        raise Exception(
                            'ERROR: Could not process record. More columns than expected in weekly file')
    else:
        with open(weeklyAndDailyPath + '_temp', 'w', encoding='utf8') as withDaily:
            with open(weeklyAndDailyPath, 'r', encoding='utf8') as weekly:
                for line in weekly:
                    cols = line.split('|')
                    if cols[1] not in ids_to_remove:
                        withDaily.write(line)
        os.remove(weeklyAndDailyPath)
        os.rename(weeklyAndDailyPath + '_temp', weeklyAndDailyPath)


def updateIndividualFile(dayFile, directory, lineBuffer):
    """Append daily data buffer to combined data file."""
    weeklyAndDailyPath = directory + '/weekly/' + dayFile + '_withDaily'
    if os.path.isfile(weeklyAndDailyPath):
        with open(weeklyAndDailyPath, 'a', encoding='utf8') as withDaily:
            withDaily.write(lineBuffer)
    else:
        raise Exception('Combined file ' + weeklyAndDailyPath + ' does not exist')


def readEntries(dayFile, directory, day, versionIdx):
    """Read daily file and create well-formed entries."""
    recordBuffer = ''
    idsToRemove = []
    with open(directory + '/' + day + '/' + dayFile, encoding='utf8') as infile:
        numExpectedCols = neededFilesUS[versionIdx][dayFile]
        record = ''
        symbolCount = 0
        linenum = 0
        for line in infile:
            linenum += 1
            line = line.replace('\n', '').replace('\r', '')
            if line == '' or line == ' ':
                continue
            elif '|' not in line:
                record += line
                continue
            else:
                symbolCount += line.count('|')
                record += line
            if symbolCount == numExpectedCols:
                cols = record.split('|')
                fccId = cols[1]
                if fccId not in idsToRemove:
                    idsToRemove.append(fccId)
                recordBuffer += record + '\r\n'
                record = ''
                symbolCount = 0
            elif symbolCount > numExpectedCols:
                raise Exception(
                    'ERROR: Could not process record more columns than expected: ' +
                    day + '/' + dayFile + ':' + str(linenum))
    removeFromCombinedFile(dayFile, directory, idsToRemove, day, versionIdx)
    updateIndividualFile(dayFile, directory, recordBuffer)


def processDailyFiles(weeklyCreation, logFile, directory, currentWeekday):
    """Process daily files replacing weekly entries where needed."""
    logFile.write('Processing daily files\n')
    upload_time = weeklyCreation

    if weeklyCreation >= versionTime:
        versionIdx = 1
    else:
        versionIdx = 0
    for file in list(neededFilesUS[versionIdx].keys()):
        removeFromCombinedFile(file, directory, [], 'weekly', versionIdx)

    for key, day in list(dayMap.items()):
        dayDirectory = directory + '/' + day
        fileCreationDate = verifyCountsFile(dayDirectory)
        timeDiff = fileCreationDate - weeklyCreation
        if timeDiff.total_seconds() > 0:
            upload_time = fileCreationDate
            if fileCreationDate >= versionTime:
                versionIdx = 1
            else:
                versionIdx = 0
            for dailyFile in os.listdir(dayDirectory):
                if dailyFile in list(neededFilesUS[versionIdx].keys()):
                    logFile.write('Processing ' + dailyFile + ' for: ' + day + '\n')
                    readEntries(dailyFile, directory, day, versionIdx)
        else:
            logFile.write(
                'INFO: Skipping ' + day +
                ' files because they are older than the weekly file\n')

        if key == currentWeekday:
            break
    return upload_time


def generateUlsScriptInputUS(directory, logFile, genFilename):
    """Format and concatenate US dataset into coalition script input."""
    logFile.write('Appending US data to ' + genFilename +
                  ' as input for uls script\n')
    with open(genFilename, 'a', encoding='utf8') as combined:
        for weeklyFile in os.listdir(directory):
            if "withDaily" in weeklyFile:
                logFile.write('Adding ' + directory + '/' +
                              weeklyFile + ' to ' + genFilename + '\n')
                with open(directory + '/' + weeklyFile, 'r', encoding='utf8') as infile:
                    for line in infile:
                        combined.write('US:' + line)


def generateUlsScriptInputCA(directory, logFile, genFilename):
    """Format and concatenate Canadian dataset and return MD5 identity digest."""
    logFile.write('Appending CA data to ' + genFilename +
                  ' as input for uls script\n')
    sourceFilenames = []
    with open(genFilename, 'a', encoding='utf8') as combined:
        for dataFile in os.listdir(directory):
            if fnmatch.fnmatch(dataFile, "??.csv") and os.path.isfile(
                    os.path.join(directory, dataFile)):
                sourceFilenames.append(dataFile)
            if dataFile != "AP.csv":
                logFile.write('Adding ' + directory + '/' +
                              dataFile + ' to ' + genFilename + '\n')
                with open(directory + '/' + dataFile, 'r', encoding='utf8') as csvfile:
                    code = dataFile.replace('.csv', '')
                    csvreader = csv.reader(csvfile)
                    for row in csvreader:
                        for (i, field) in enumerate(row):
                            row[i] = field.replace('|', ':')
                        combined.write('CA:' + code + '|' +
                                       ('|'.join(row)) + '|\n')
    if not sourceFilenames:
        raise Exception("CA source filenames not found")
    sources_md5 = hashlib.md5(usedforsecurity=False)
    for sourceFilename in sorted(sourceFilenames):
        with open(os.path.join(directory, sourceFilename), mode="rb") as f:
            sources_md5.update(f.read())
    return sources_md5.hexdigest()


def generateUlsScriptInputStatic(staticDataFile, logFile, genFilename):
    """Format and append static FS dataset into coalition script input."""
    logFile.write('Appending Static FS data data to ' + genFilename +
                  ' as input for uls script\n')
    with open(genFilename, 'a', encoding='utf8') as combined:
        logFile.write('Adding ' + staticDataFile + ' to ' + genFilename + '\n')
        with open(staticDataFile, 'r', encoding='utf8') as csvfile:
            csvreader = csv.reader(csvfile)
            for row in csvreader:
                for (i, field) in enumerate(row):
                    row[i] = field.replace('|', ':')
                combined.write(('|'.join(row)) + '|\n')


def storeDataIdentities(sqlFile, identityDict):
    """Store region data identities in generated SQLite database.

    For FCC ULS identity is upload datetime, for Canada SMS is MD5 digest.
    """
    assert os.path.isfile(sqlFile)
    engine = sa.create_engine("sqlite:///" + sqlFile)
    metadata = sa.MetaData()
    metadata.reflect(bind=engine)
    conn = engine.connect()
    if not sa.inspect(engine).has_table("data_ids"):
        sa.Table("data_ids", metadata,
                 sa.Column("region", sa.String(100), primary_key=True),
                 sa.Column("identity", sa.String(1000), nullable=False))
        metadata.create_all(engine)
    idsTable = metadata.tables["data_ids"]
    for region in sorted(identityDict.keys()):
        conn.execute(sa.insert(idsTable).values(region=region,
                                                identity=identityDict[region]))
    if hasattr(conn, 'commit'):
        conn.commit()
    conn.close()
