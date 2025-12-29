#!/usr/bin/env python
import os
import datetime
import zipfile
import shutil
import subprocess
import sys
import glob
import argparse
import csv
import urllib.request
import urllib.parse
import urllib.error
from collections import OrderedDict
import ssl
from urllib.error import URLError
from processAntennaCSVs import processAntFiles
from csvToSqliteULS import convertULS
# from sort_callsigns_all import sortCallsigns
from sort_callsigns_addfsid import sortCallsignsAddFSID
from fix_bps import fixBPS
from fix_params import fixParams
import sqlalchemy as sa
import hashlib
import fnmatch

ssl._create_default_https_context = ssl._create_unverified_context

# file types we need to consider along with their # of | symbols (i.e. #
# of cols - 1)
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
    'SG.dat': 14}
neededFilesUS[1] = {
    'AN.dat': 37,
    'CP.dat': 13,
    'EM.dat': 15,
    'EN.dat': 29,
    'FR.dat': 29,
    'HD.dat': 58,
    'LO.dat': 50,
    'PA.dat': 23,
    'SG.dat': 14}

# Version changed AUG 18, 2022
versionTime = datetime.datetime(2022, 8, 18, 0, 0, 0)

# map to reuse weekday in loops
dayMap = OrderedDict()  # ordered dictionary so order is mainatined
dayMap[6] = 'sun'
dayMap[0] = 'mon'
dayMap[1] = 'tue'
dayMap[2] = 'wed'
dayMap[3] = 'thu'
dayMap[4] = 'fri'
dayMap[5] = 'sat'
# map to reuse for converting month strings to ints
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
    'dec': 12
}

###############################################################################
# Download data files for each region (US, CA)                                #
# Currently in fullPathTempDir                                                #
###############################################################################


def downloadFiles(region, logFile, currentWeekday, fullPathTempDir):
    regionDataDir = fullPathTempDir + '/' + region
    if (not os.path.isdir(regionDataDir)):
        os.mkdir(regionDataDir)
    logFile.write('Downloading data files for ' +
                  region + ' into ' + regionDataDir + '\n')
    if region == 'US':
        # download the latest Weekly Update
        weeklyURL = 'https://data.fcc.gov/download/pub/uls/complete/l_micro.zip'
        logFile.write('Downloading weekly' + '\n')
        urllib.request.urlretrieve(weeklyURL, regionDataDir + '/weekly.zip')

        # download all the daily updates starting from Sunday up to that day
        # example: on Wednesday morning, we will download the weekly update
        # PLUS Sun, Mon, Tue and Wed daily updates
        for key, day in dayMap.items():
            dayStr = day
            dailyURL = 'https://data.fcc.gov/download/pub/uls/daily/l_mw_' + dayStr + '.zip'
            logFile.write('Downloading ' + dayStr + '\n')
            urllib.request.urlretrieve(
                dailyURL, regionDataDir + '/' + dayStr + '.zip')
            # Exit after processing today's file
            if (key == currentWeekday) and (day != 'sun'):
                break
    elif region == 'CA':
        urllib.request.urlretrieve(
            'https://www.ic.gc.ca/engineering/Stations_Data_Extracts.csv',
            regionDataDir + '/SD.csv')
        urllib.request.urlretrieve(
            'https://www.ic.gc.ca/engineering/Passive_Repeater_data_extract.csv',
            regionDataDir + '/PP.csv')
        urllib.request.urlretrieve(
            'https://www.ic.gc.ca/engineering/Passive_Reflectors_Data_Extract.csv',
            regionDataDir + '/PR.csv')

        # Not processing transmitters for CA
        # urllib.request.urlretrieve('https://www.ic.gc.ca/engineering/SMS_TAFL_Files/TAFL_LTAF_Fixe.zip',   'TAFL_LTAF_Fixe.zip')
        # zip_file = zipfile.ZipFile("TAFL_LTAF_Fixe.zip") # zip object
        # zip_file.extractall(regionDataDir)
        # zip_file.close()
        # if os.path.isfile(regionDataDir + '/TAFL_LTAF_Fixe.csv'):
        #     os.rename(regionDataDir + '/TAFL_LTAF_Fixe.csv', regionDataDir + '/TA.csv')
        # elif os.path.isfile(regionDataDir + '/IC_TAFL_File_fixed.csv'):
        #     os.rename(regionDataDir + '/IC_TAFL_File_fixed.csv', regionDataDir + '/TA.csv')
        # else:
        #     raise Exception('ERROR: Unable to process CA file {}'.format("TAFL_LTAF_Fixe.zip"))

        cmd = 'echo "Antenna Manufacturer,Antenna Model Number,Antenna Gain [dBi],Antenna Diameter,Beamwidth [deg],Last Updated,Pattern Type,Pattern Azimuth [deg],Pattern Attenuation [dB]" >| ' + regionDataDir + '/AP.csv'  # noqa
        os.system(cmd)
        urllib.request.urlretrieve(
            'https://www.ic.gc.ca/engineering/Antenna_Patterns_6GHz.csv',
            regionDataDir + '/Antenna_Patterns_6GHz_orig.csv')
        cmd = 'cat ' + regionDataDir + \
            '/Antenna_Patterns_6GHz_orig.csv >> ' + regionDataDir + '/AP.csv'
        os.system(cmd)
        os.remove(regionDataDir + '/Antenna_Patterns_6GHz_orig.csv')
###############################################################################

# Downloads antenna files


def prepareAFCGitHubFiles(rawDir, destDir, logFile):
    # Files in rawDir are downloaded from
    # https://raw.githubusercontent.com/Wireless-Innovation-Forum/6-GHz-AFC/main/data/common_data/
    # (can also be viewed at https://github.com/Wireless-Innovation-Forum/6-GHz-AFC/tree/main/data/common_data)
    # This function brings them to palatable state in destDir
    dataFileList = ['antenna_model_diameter_gain.csv',
                    'billboard_reflector.csv',
                    'category_b1_antennas.csv',
                    'high_performance_antennas.csv',
                    'fcc_fixed_service_channelization.csv',
                    'transmit_radio_unit_architecture.csv',
                    ]

    for dataFile in dataFileList:
        srcFile = os.path.join(rawDir, dataFile)
        dstFile = os.path.join(destDir, dataFile)
        # Remove control characters.
        cmd = 'tr -d \'\\200-\\377\\015\' < ' + srcFile + ' '
        # Remove blank lines.
        # cmd += '| gawk -F "," \'($2 != "") { print }\' ' \
        # Fix spelling error.
        # cmd += '| sed \'s/daimeter/diameter/\' ' \

        cmd += '>| ' + dstFile
        os.system(cmd)
        if dataFile == "fcc_fixed_service_channelization.csv":
            cmd = 'echo -e "5967.4375,30,\n' \
                + '6056.3875,30,\n' \
                + '6189.8275,30,\n' \
                + '6219.4775,30,\n' \
                + '6308.4275,30," >> ' + dstFile
            os.system(cmd)

# Extracts all the zip files into sub-directories


def extractZips(logFile, directory):
    logFile.write('Extracting zips for directory ' + directory + '\n')
    # unzip into sub-directories of directory
    for tempZip in os.listdir(directory):  # unzip every zip
        if (tempZip.endswith('.zip')):
            logFile.write('Extracting ' + tempZip + '\n')
            fileName = os.path.abspath(
                directory + '/' + tempZip)  # get full path
            zip_file = zipfile.ZipFile(fileName)  # zip object
            subDirName = fileName.replace('.zip', '')
            os.mkdir(subDirName)  # sub-directory to extract to
            zip_file.extractall(subDirName)
            zip_file.close()

# Returns the datetime object based on the counts file


def verifyCountsFile(directory):
    # check weekly date
    with open(directory + '/counts', 'r') as countsFile:
        line = countsFile.readline()
        # FCC Format for the line is:
        # File Creation Date: Sun Oct  3 17:59:04 EDT 2021
        # remove non-date part of string
        dateStr = line.replace('File Creation Date: ', '')
        dateData = dateStr.split()  # split into word array
        weekday = dateData[0]
        # convert month string to an int between 1 and 12
        month = monthMap.get(dateData[1].lower(), 'err')
        day = int(dateData[2])  # day of the month as a Number
        time = dateData[3]
        timeZone = dateData[4]
        year = int(dateData[5])
        # convert string time to numbers array
        timeData = [int(string) for string in time.split(':')]
        hours = timeData[0]
        mins = timeData[1]
        sec = timeData[2]
        if (month != 'err'):
            fileCreationDate = datetime.datetime(
                year, month, day, hours, mins, sec)
            return fileCreationDate
        else:
            raise Exception(
                'ERROR: Could not parse month of FCC string in counts file for ' +
                directory +
                ' update')


# Removes any record with the given id from the given file
def removeFromCombinedFile(
        fileName, directory, ids_to_remove, day, versionIdx):
    weeklyAndDailyPath = directory + '/weekly/' + fileName + '_withDaily'

    if (day == 'weekly'):
        # create file that contains weekly and daily
        with open(weeklyAndDailyPath, 'w', encoding='utf8') as withDaily:
            # open weekly file
            with open(directory + '/weekly/' + fileName, 'r', encoding='utf8') as weekly:
                record = ''
                symbolCount = 0
                numExpectedCols = neededFilesUS[versionIdx][fileName]
                for line in weekly:
                    # remove newline characters from line
                    line = line.replace('\n', '')
                    line = line.replace('\r', '')
                    # the line was just newline character(s), skip it
                    if (line == '' or line == ' '):
                        continue
                    # the line is a single piece of data that does not contain
                    # the | character
                    elif ('|' not in line):
                        record += line
                        continue
                    else:
                        # this many | were in the line
                        symbolCount += line.count('|')
                        record += line
                    # the record is complete if the number of | symbols is
                    # equal to the number of expected cols
                    if (symbolCount == numExpectedCols):
                        cols = record.split('|')
                        fileType = cols[0]
                        # Ensure we need this entry
                        if (fileType +
                                ".dat" in list(neededFilesUS[versionIdx].keys())):
                            # only write when the id is not in the list of ids
                            if (not cols[1] in ids_to_remove):
                                record += "\r\n"  # add newline
                                withDaily.write(record)
                            # reset for the next record
                            record = ''
                            symbolCount = 0
                    elif (symbolCount > numExpectedCols):
                        e = Exception(
                            'ERROR: Could not process record. More columns than expected in weekly file')
                        raise e
    else:
        # open new file that contains weekly and daily
        with open(weeklyAndDailyPath + '_temp', 'w', encoding='utf8') as withDaily:
            # open older file
            with open(weeklyAndDailyPath, 'r', encoding='utf8') as weekly:
                for line in weekly:
                    cols = line.split('|')
                    # only write when the id is not in the list of ids
                    if (not cols[1] in ids_to_remove):
                        withDaily.write(line)
        # remove old combined, move new one to right place
        os.remove(weeklyAndDailyPath)
        os.rename(weeklyAndDailyPath + '_temp', weeklyAndDailyPath)

# Update specific datafile with daily data


def updateIndividualFile(dayFile, directory, lineBuffer):
    weeklyAndDailyPath = directory + '/weekly/' + dayFile + '_withDaily'
    if (os.path.isfile(weeklyAndDailyPath)):
        # open file that contains weekly and daily
        with open(weeklyAndDailyPath, 'a', encoding='utf8') as withDaily:
            withDaily.write(lineBuffer)
    else:
        e = Exception('Combined file ' +
                      weeklyAndDailyPath + ' does not exist')
        raise e

# Reads the file and creates well formed entries from FCC data.
# The ONLY thing that consititutes a valid entry is the number of |
# characters (e.g. AN files have 38 columns and thus 37 | per record)


def readEntries(dayFile, directory, day, versionIdx):
    recordBuffer = ''  # buffer to limit number of file open() calls
    idsToRemove = []
    with open(directory + '/' + day + '/' + dayFile, encoding='utf8') as infile:
        numExpectedCols = neededFilesUS[versionIdx][dayFile]
        record = ''
        symbolCount = 0
        # Iterate over the lines in the file
        linenum = 0
        for line in infile:
            linenum += 1
            # remove newline characters from line
            line = line.replace('\n', '')
            line = line.replace('\r', '')

            # the line we were given was just newline character(s) or
            # whitespace, skip it
            if (line == '' or line == ' '):
                continue
            # the line is a single piece of data that does not contain the |
            # character
            elif ('|' not in line):
                record += line
                continue
            else:
                symbolCount += line.count('|')  # this many | were in the line
                record += line
            # the record is complete if the number of | symbols is equal to the
            # number of expected cols
            if (symbolCount == numExpectedCols):
                cols = record.split('|')
                # FCC unique system identifier should always be this index
                fccId = cols[1]
                # only need to remove an ID once per file per day
                if (fccId not in idsToRemove):
                    idsToRemove.append(fccId)
                # store the record to the buffer with proper newline for csv
                # format
                recordBuffer += record + '\r\n'
                record = ''  # reset the record
                symbolCount = 0
            elif (symbolCount > numExpectedCols):
                raise Exception(
                    'ERROR: Could not process record more columns than expected: ' +
                    day +
                    '/' +
                    dayFile +
                    ':' +
                    str(linenum))
    removeFromCombinedFile(dayFile, directory, idsToRemove, day, versionIdx)
    updateIndividualFile(dayFile, directory, recordBuffer)

# Processes the daily files, replacing weekly entries when needed
# Returns datetime of ULS data upload (ULS data identity)


def processDailyFiles(weeklyCreation, logFile, directory, currentWeekday):
    logFile.write('Processing daily files' + '\n')

    # Most recent timetag from 'counts'
    upload_time = weeklyCreation

    # Process weekly file
    if (weeklyCreation >= versionTime):
        versionIdx = 1
    else:
        versionIdx = 0
    for file in list(neededFilesUS[versionIdx].keys()):
        # removeFromCombinedFile() will fix any formatting in FCC data
        # passing an empty list for second arg means no records will be removed
        removeFromCombinedFile(file, directory, [], 'weekly', versionIdx)

    for key, day in list(dayMap.items()):
        dayDirectory = directory + '/' + day

        # ensure counts file is newer than weekly
        fileCreationDate = verifyCountsFile(dayDirectory)
        timeDiff = fileCreationDate - weeklyCreation
        if (timeDiff.total_seconds() > 0):
            upload_time = fileCreationDate
            if (fileCreationDate >= versionTime):
                versionIdx = 1
            else:
                versionIdx = 0
            for dailyFile in os.listdir(dayDirectory):
                if (dailyFile in list(neededFilesUS[versionIdx].keys())):
                    logFile.write('Processing ' + dailyFile +
                                  ' for: ' + day + '\n')
                    readEntries(dailyFile, directory, day, versionIdx)
        else:
            logFile.write(
                'INFO: Skipping ' +
                day +
                ' files because they are older than the weekly file' +
                '\n')

        # Exit after processing yesterdays file
        if (key == currentWeekday):
            break
    return upload_time

# Generates the combined text file that the coalition processor uses.


def generateUlsScriptInputUS(directory, logFile, genFilename):
    logFile.write('Appending US data to ' + genFilename +
                  ' as input for uls script' + '\n')
    with open(genFilename, 'a', encoding='utf8') as combined:
        for weeklyFile in os.listdir(directory):
            if "withDaily" in weeklyFile:
                logFile.write('Adding ' + directory + '/' +
                              weeklyFile + ' to ' + genFilename + '\n')
                with open(directory + '/' + weeklyFile, 'r', encoding='utf8') as infile:
                    for line in infile:
                        combined.write('US:' + line)


def generateUlsScriptInputCA(directory, logFile, genFilename):
    """ Returns identity string of downloaded data """
    logFile.write('Appending CA data to ' + genFilename +
                  ' as input for uls script' + '\n')
    # Names of source files (to compute MD5 of)
    sourceFilenames = []
    with open(genFilename, 'a', encoding='utf8') as combined:
        for dataFile in os.listdir(directory):
            if fnmatch.fnmatch(dataFile, "??.csv") and os.path.isfile(
                    os.path.join(directory, dataFile)):
                sourceFilenames.append(dataFile)
            if dataFile != "AP.csv":  # skip antenna pattern file, processed separately
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
    sources_md5 = hashlib.md5()
    for sourceFilename in sorted(sourceFilenames):
        with open(os.path.join(directory, sourceFilename), mode="rb") as f:
            sources_md5.update(f.read())
    return sources_md5.hexdigest()


def generateUlsScriptInputStatic(staticDataFile, logFile, genFilename):
    logFile.write('Appending Static FS data data to ' + genFilename +
                  ' as input for uls script' + '\n')
    # Names of source files (to compute MD5 of)
    with open(genFilename, 'a', encoding='utf8') as combined:
        logFile.write('Adding ' + staticDataFile + ' to ' + genFilename + '\n')
        with open(staticDataFile, 'r', encoding='utf8') as csvfile:
            csvreader = csv.reader(csvfile)
            for row in csvreader:
                for (i, field) in enumerate(row):
                    row[i] = field.replace('|', ':')
                combined.write(('|'.join(row)) + '|\n')
    return


def storeDataIdentities(sqlFile, identityDict):
    """ Stores region data identities in gewnerated SQLite database.
    For FCC ULS identity is upload datetime, for Canada SMS, for now, sadly,
    only an MD5 of downloaded files.

    Arguments:
    sqlFile      -- SQLite file where to add table with region identities
    identityDict -- Dictionary of region identities. Keys are region names (US,
                    CA, ...), values are identity strings
    """
    assert os.path.isfile(sqlFile)
    engine = sa.create_engine("sqlite:///" + sqlFile)
    metadata = sa.MetaData()
    metadata.reflect(bind=engine)
    conn = engine.connect()
    if not sa.inspect(engine).has_table("data_ids"):
        dataIdsTable = \
            sa.Table("data_ids", metadata,
                     sa.Column("region", sa.String(100), primary_key=True),
                     sa.Column("identity", sa.String(1000), nullable=False))
        metadata.create_all(engine)
    idsTable = metadata.tables["data_ids"]
    for region in sorted(identityDict.keys()):
        conn.execute(sa.insert(idsTable).values(region=region,
                                                identity=identityDict[region]))
    conn.close()


def daily_uls_parse(state_root, interactive):
    startTime = datetime.datetime.now()
    nameTime = startTime.isoformat().replace(":", '_')

    nameTime += "_UniiUS" + uniiStr.replace(":", "")

    temp = "/temp"

    root = state_root + "/daily_uls_parse"  # root so path is consisent

    ###########################################################################
    # If interactive, prompt to set root path                                 #
    ###########################################################################
    if interactive:
        print("Specify full path for root daily_uls_parse dir")
        value = input("Enter Directory (" + root + "): ")
        if (value != ""):
            root = value
        print("daily_uls_parse root directory set to " + root)
    ###########################################################################

    # weekday() is 0 indexed at monday
    currentWeekday = datetime.datetime.today().weekday()

    ###########################################################################
    # If interactive, prompt for weekday                                      #
    ###########################################################################
    if wfaFlag:
        currentWeekday = 0
    elif interactive:
        print("Enter Current Weekday for FCC files: ")
        for key, day in list(dayMap.items()):
            print(str(key) + ": " + day)
        value = input("Current Weekday (" + str(currentWeekday) + "): ")
        if (value != ""):
            currentWeekday = int(value)
        if (currentWeekday < 0 or currentWeekday > 6):
            print("ERROR: currentWeekday = " +
                  str(currentWeekday) + " invalid, must be in [0,6]")
            return
    ###########################################################################

    fullPathTempDir = root + temp

    ###########################################################################
    # If interactive, prompt for removal of temp directory                    #
    ###########################################################################
    if wfaFlag:
        removeTempDirFlag = False
    elif interactive:
        accepted = False
        while not accepted:
            value = input("Remove temp directory: " +
                          fullPathTempDir + " ? (y/n): ")
            if value == "y":
                accepted = True
                removeTempDirFlag = True
            elif value == "n":
                accepted = True
                removeTempDirFlag = False
            else:
                print("ERROR: Invalid input: " + value + ", must be y or n")
    else:
        removeTempDirFlag = True
    ###########################################################################

    ###########################################################################
    # If removeTempDirFlag set, remove temp dir, otherwise must already exist #
    ###########################################################################
    if removeTempDirFlag:
        if (os.path.isdir(fullPathTempDir)):
            try:
                shutil.rmtree(fullPathTempDir)  # delete temp folder
            except Exception as e:
                # LOGGER.error('ERROR: Could not delete old temp directory:')
                raise e
        # create temp directory to download files to
        os.mkdir(fullPathTempDir)
    ###########################################################################

    ###########################################################################
    # cd to temp dir and begin creating log file                              #
    ###########################################################################
    if (not os.path.isdir(fullPathTempDir)):
        print("ERROR: " + fullPathTempDir + " does not exist")
        return

    os.chdir(fullPathTempDir)  # change to temp
    logname = fullPathTempDir + "/dailyParse_" + nameTime + ".log"
    logFile = open(logname, 'w', 1)
    if interactive:
        logFile.write('Starting interactive mode update at: ' +
                      startTime.isoformat() + '\n')
    else:
        logFile.write('Starting update at: ' + startTime.isoformat() + '\n')
    ###########################################################################

    for region in regionList:

        #######################################################################
        # If interactive, prompt for downloading of data files for region         #
        #######################################################################
        if wfaFlag:
            downloadDataFilesFlag = False
        elif interactive:
            accepted = False
            while not accepted:
                value = input("Download data files for " +
                              region + "? (y/n): ")
                if value == "y":
                    accepted = True
                    downloadDataFilesFlag = True
                elif value == "n":
                    accepted = True
                    downloadDataFilesFlag = False
                else:
                    print("ERROR: Invalid input: " +
                          value + ", must be y or n")
        else:
            downloadDataFilesFlag = True
        #######################################################################

        #######################################################################
        # If downloadDataFilesFlag set, download data files for region            #
        #######################################################################
        if downloadDataFilesFlag:
            downloadFiles(region, logFile, currentWeekday, fullPathTempDir)
        #######################################################################

        regionDataDir = fullPathTempDir + '/' + region

        if region == 'US':
            ###################################################################
            # If interactive, prompt for extraction of files from zip files           #
            ###################################################################
            if wfaFlag:
                extractZipFlag = True
            elif interactive:
                value = input(
                    "Extract FCC files from downloaded zip files? (y/n): ")
                if value == "y":
                    extractZipFlag = True
                elif value == "n":
                    extractZipFlag = False
                else:
                    print("ERROR: Invalid input: " +
                          value + ", must be y or n")
            else:
                extractZipFlag = True
            ###################################################################

            ###################################################################
            # If extractZipFlag set, extract files from zip files                     #
            ###################################################################
            if extractZipFlag:
                extractZips(logFile, regionDataDir)
            ###################################################################

    ###########################################################################
    # If interactive, prompt for converting AFC GitHub data files            #
    ###########################################################################
    if wfaFlag:
        prepareAFCGitHubFilesFlag = False
    elif interactive:
        accepted = False
        while not accepted:
            value = input("Prepare AFC GitHub data files? (y/n): ")
            if value == "y":
                accepted = True
                prepareAFCGitHubFilesFlag = True
            elif value == "n":
                accepted = True
                prepareAFCGitHubFilesFlag = False
            else:
                print("ERROR: Invalid input: " + value + ", must be y or n")
    else:
        prepareAFCGitHubFilesFlag = True
    ###########################################################################

    ###########################################################################
    # If prepareAFCGitHubFilesFlag set, prepare AFC GitHub data files         #
    ###########################################################################
    if prepareAFCGitHubFilesFlag:
        prepareAFCGitHubFiles(
            root + '/raw_wireless_innovation_forum_files', ".", logFile)
    ###########################################################################

    ###########################################################################
    # If interactive, prompt for creating antenna_model_list.csv              #
    ###########################################################################
    if wfaFlag:
        processAntFilesFlag = True
    elif interactive:
        accepted = False
        while not accepted:
            value = input(
                "Process antenna model files to create antenna_model_list.csv, antenna_prefix_list.csv and antennaPatternFile? (y/n): ")
            if value == "y":
                accepted = True
                processAntFilesFlag = True
            elif value == "n":
                accepted = True
                processAntFilesFlag = False
            else:
                print("ERROR: Invalid input: " + value + ", must be y or n")
    else:
        processAntFilesFlag = True
    ###########################################################################

    antennaPatternFileFile = 'afc_antenna_patterns_' + nameTime + '.csv'

    ###########################################################################
    # If interactive, prompt to set antennaPatternFileFile, note that         #
    # if processAntFilesFlag is not set, this file should exist for           #
    # subsequent processing.                                                  #
    ###########################################################################
    if interactive:
        if not processAntFilesFlag:
            flist = glob.glob(
                fullPathTempDir +
                "/afc_antenna_patterns_[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]T[0-9][0-9]_[0-9][0-9]_[0-9][0-9].[0-9][0-9][0-9][0-9][0-9][0-9].csv")  # noqa
            if (len(flist)):
                antennaPatternFileFile = os.path.basename(flist[-1])

        value = input("Enter Antenna Pattern filename (" +
                      antennaPatternFileFile + "): ")
        if (value != ""):
            antennaPatternFileFile = value
    ###########################################################################

    # output filename from uls-script
    fullPathAntennaPatternFile = fullPathTempDir + "/" + antennaPatternFileFile

    ###########################################################################
    # If processAntFilesFlag set, process data files to create                #
    # antenna_model_list.csv                                                  #
    ###########################################################################
    if processAntFilesFlag:
        processAntFiles(fullPathTempDir, processCA, combineAntennaRegionFlag,
                        fullPathTempDir + '/antenna_model_list.csv',
                        fullPathTempDir + '/antenna_prefix_list.csv',
                        fullPathAntennaPatternFile, logFile)
    ###########################################################################

    ###########################################################################
    # If interactive, prompt for processing download files for each region    #
    # to create combined.txt                                                  #
    ###########################################################################
    if interactive:
        accepted = False
        while not accepted:
            value = input(
                "Process FCC files and generate file combined.txt to use as input to uls-script? (y/n): ")
            if value == "y":
                accepted = True
                processDownloadFlag = True
            elif value == "n":
                accepted = True
                processDownloadFlag = False
            else:
                print("ERROR: Invalid input: " + value + ", must be y or n")
    else:
        processDownloadFlag = True
    ###########################################################################

    # input filename for uls-script
    fullPathCoalitionScriptInput = fullPathTempDir + "/combined.txt"

    # Per region data identities - upload date, hash, etc.
    dataIdentities = {}

    ###########################################################################
    # If processDownloadFlag set, process Download files to create combined.txt         #
    ###########################################################################
    if processDownloadFlag:
        with open(fullPathCoalitionScriptInput, 'w', encoding='utf8') as combined:
            pass  # Do nothing, create empty file that will be appended to

        for region in regionList:

            regionDataDir = fullPathTempDir + '/' + region
            dataIdentity = None

            if region == 'US':
                # US files (FCC) consist of weekly and daily updates.
                # get the time creation of weekly file from the counts file
                weeklyCreation = verifyCountsFile(regionDataDir + '/weekly')
                # process the daily files day by day
                uploadTime = processDailyFiles(
                    weeklyCreation, logFile, regionDataDir, currentWeekday)
                # For US identity is FCC ULS upoload datetime
                dataIdentity = uploadTime.isoformat()

                rasDataFileUSSrc = root + '/data_files/RASdatabase.dat'
                rasDataFileUSTgt = regionDataDir + '/weekly/RA.dat_withDaily'
                logFile.write("Copying " + rasDataFileUSSrc +
                              ' to ' + rasDataFileUSTgt + '\n')
                subprocess.call(['cp', rasDataFileUSSrc, rasDataFileUSTgt])

                # generate the combined csv/txt file for the coalition uls
                # processor
                generateUlsScriptInputUS(
                    regionDataDir + '/weekly',
                    logFile,
                    fullPathCoalitionScriptInput)
            elif region == 'CA':
                # For Canada identity is MD5 of downloaded files
                dataIdentity = generateUlsScriptInputCA(
                    regionDataDir, logFile, fullPathCoalitionScriptInput)
            else:
                logFile.write('ERROR: Invalid region = ' + region)
                raise e
            assert dataIdentity is not None
            dataIdentities[region] = dataIdentity

        staticDataFile = root + '/data_files/static_fs_database.csv'

        if os.path.isfile(staticDataFile):
            generateUlsScriptInputStatic(staticDataFile, logFile, fullPathCoalitionScriptInput)

    ###########################################################################

    ###########################################################################
    # If interactive, prompt for running ULS Processor (uls-script)           #
    ###########################################################################
    if wfaFlag:
        runULSProcessorFlag = True
    elif interactive:
        accepted = False
        while not accepted:
            value = input("Run ULS Processor, uls-script? (y/n): ")
            if value == "y":
                accepted = True
                runULSProcessorFlag = True
            elif value == "n":
                accepted = True
                runULSProcessorFlag = False
            else:
                print("ERROR: Invalid input: " + value + ", must be y or n")
    else:
        runULSProcessorFlag = True
    ###########################################################################

    # os.chdir(root) # change back to root of this script
    coalitionScriptOutputFSFilename = 'FS_' + nameTime + '.csv'
    coalitionScriptOutputRASFilename = 'RAS_' + nameTime + '.csv'

    ###########################################################################
    # If interactive, prompt to set output file from ULS Processor, note that #
    # if the ULS Processor is not going to be run, this file should exist for #
    # subsequent processing.                                                  #
    ###########################################################################
    if interactive:
        if not runULSProcessorFlag:
            flist = glob.glob(
                fullPathTempDir +
                "/FS_[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]T[0-9][0-9]_[0-9][0-9]_[0-9][0-9].[0-9][0-9][0-9][0-9][0-9][0-9].csv")
            if (len(flist)):
                coalitionScriptOutputFSFilename = os.path.basename(flist[-1])
            flist = glob.glob(
                fullPathTempDir +
                "/RAS_[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]T[0-9][0-9]_[0-9][0-9]_[0-9][0-9].[0-9][0-9][0-9][0-9][0-9][0-9].csv")
            if (len(flist)):
                coalitionScriptOutputRASFilename = os.path.basename(flist[-1])

        value = input("Enter ULS Processor output FS filename (" +
                      coalitionScriptOutputFSFilename + "): ")
        if (value != ""):
            coalitionScriptOutputFSFilename = value

        value = input("Enter ULS Processor output RAS filename (" +
                      coalitionScriptOutputRASFilename + "): ")
        if (value != ""):
            coalitionScriptOutputRASFilename = value
    ###########################################################################

    # output filename from uls-script
    fullPathCoalitionScriptOutput = fullPathTempDir + \
        "/" + coalitionScriptOutputFSFilename
    fullPathRASDabataseFile = fullPathTempDir + \
        "/" + coalitionScriptOutputRASFilename

    ###########################################################################
    # If runULSProcessorFlag set, run ULS processor                           #
    ###########################################################################
    if runULSProcessorFlag:
        mode = "proc_uls"
        if combineAntennaRegionFlag:
            mode += "_ca"

        # run through the uls processor
        logFile.write('Running through ULS processor' + '\n')
        try:
            subprocess.call([root + '/uls-script',
                             fullPathTempDir + '/combined.txt',
                             fullPathCoalitionScriptOutput,
                             fullPathRASDabataseFile,
                             fullPathTempDir + '/antenna_model_list.csv',
                             fullPathTempDir + '/antenna_prefix_list.csv',
                             root + '/antenna_model_map.csv',
                             fullPathTempDir + '/fcc_fixed_service_channelization.csv',
                             fullPathTempDir + '/transmit_radio_unit_architecture.csv',
                             uniiStr,
                             mode])
        except Exception as e:
            logFile.write('ERROR: ULS processor error:')
            raise e
    ###########################################################################

    ###########################################################################
    # If interactive, prompt for running fixBPS                               #
    ###########################################################################
    if wfaFlag:
        runFixBPSFlag = True
    elif interactive:
        accepted = False
        while not accepted:
            value = input("Run fixBPS? (y/n): ")
            if value == "y":
                accepted = True
                runFixBPSFlag = True
            elif value == "n":
                accepted = True
                runFixBPSFlag = False
            else:
                print("ERROR: Invalid input: " + value + ", must be y or n")
    else:
        runFixBPSFlag = True
    ###########################################################################

    # output filename from runBPS
    bpsScriptOutput = fullPathCoalitionScriptOutput.replace(
        '.csv', '_fixedBPS.csv')
    modcodFile = root + "/data_files/modcod_bps.csv"

    ###########################################################################
    # If runFixBPSFlag set, run fixBPS                                        #
    ###########################################################################
    if runFixBPSFlag:
        logFile.write("Running through BPS script, cwd = " +
                      os.getcwd() + '\n')
        fixBPS(fullPathCoalitionScriptOutput, modcodFile, bpsScriptOutput)
    ###########################################################################

    ###########################################################################
    # If interactive, prompt for running sortCallsignsAddFSID                 #
    ###########################################################################
    if wfaFlag:
        runSortCallsignsAddFSIDFlag = True
    elif interactive:
        accepted = False
        while not accepted:
            value = input("Run sortCallsignsAddFSID? (y/n): ")
            if value == "y":
                accepted = True
                runSortCallsignsAddFSIDFlag = True
            elif value == "n":
                accepted = True
                runSortCallsignsAddFSIDFlag = False
            else:
                print("ERROR: Invalid input: " + value + ", must be y or n")
    else:
        runSortCallsignsAddFSIDFlag = True
    ###########################################################################

    # output filename from sortCallsignsAddFSID
    # fsidTableFile is a datafile used by sortCallsignsAddFSID
    sortedOutput = bpsScriptOutput.replace(".csv", "_sorted.csv")

    ###########################################################################
    # If runSortCallsignsAddFSIDFlag set, run sortCallsignsAddFSID            #
    ###########################################################################
    if runSortCallsignsAddFSIDFlag:
        fsidTableFile = root + '/data_files/fsid_table.csv'
        fsidTableBakFile = root + '/data_files/fsid_table_bak_' + nameTime + '.csv'
        logFile.write("Backing up FSID table for to: " +
                      fsidTableBakFile + '\n')
        subprocess.call(['cp', fsidTableFile, fsidTableBakFile])
        logFile.write("Running through sort callsigns add FSID script" + '\n')
        sortCallsignsAddFSID(
            bpsScriptOutput, fsidTableFile, sortedOutput, logFile)
    ###########################################################################

    ###########################################################################
    # If interactive, prompt for running fixParams                            #
    ###########################################################################
    if wfaFlag:
        runFixParamsFlag = True
    elif interactive:
        accepted = False
        while not accepted:
            value = input("Run fixParams? (y/n): ")
            if value == "y":
                accepted = True
                runFixParamsFlag = True
            elif value == "n":
                accepted = True
                runFixParamsFlag = False
            else:
                print("ERROR: Invalid input: " + value + ", must be y or n")
    else:
        runFixParamsFlag = True
    ###########################################################################

    # output filename from fixParams
    paramOutput = sortedOutput.replace(".csv", "_param.csv")

    ###########################################################################
    # If runFixParamsFlag set, run fixParams                                  #
    ###########################################################################
    if runFixParamsFlag:
        logFile.write("Running fixParams" + '\n')
        fixParams(sortedOutput, paramOutput, logFile, False)
    ###########################################################################

    ###########################################################################
    # If interactive, prompt for running convertULS                           #
    ###########################################################################
    if wfaFlag:
        runConvertULSFlag = True
    elif interactive:
        accepted = False
        while not accepted:
            value = input("Run conversion of CSV file to sqlite? (y/n): ")
            if value == "y":
                accepted = True
                runConvertULSFlag = True
            elif value == "n":
                accepted = True
                runConvertULSFlag = False
            else:
                print("ERROR: Invalid input: " + value + ", must be y or n")
    else:
        runConvertULSFlag = True
    ###########################################################################

    # output filename from convertULS
    outputSQL = paramOutput.replace('.csv', '.sqlite3')

    ###########################################################################
    # If runConvertULSFlag set, run convertULS                                #
    ###########################################################################
    if runConvertULSFlag:
        convertULS(paramOutput, fullPathRASDabataseFile,
                   fullPathAntennaPatternFile, state_root, logFile, outputSQL)
        storeDataIdentities(outputSQL, dataIdentities)
    ###########################################################################

    finishTime = datetime.datetime.now()

    ###########################################################################
    # Record execution time in logFile and close log file                     #
    ###########################################################################
    logFile.write('Update finished at: ' + finishTime.isoformat() + '\n')
    timeDiff = finishTime - startTime
    logFile.write('Update took ' +
                  str(timeDiff.total_seconds()) + ' seconds' + '\n')
    logFile.close()
    ###########################################################################

    os.chdir(root)  # change back to root of this script

    ###########################################################################
    # If not interactive:                                                     #
    # * create zip file containing intermediate file for debugging            #
    # * copy sqlite file to ULS_Database directory for use by afc-engine      #
    ###########################################################################
    if not interactive:
        print("Creating and moving debug files\n")
        # create debug zip containing final csv, anomalous_uls, and warning_uls
        # and move it to where GUI can see
        try:
            if wfaFlag:
                dirName = "WFA_testvector_FS_" + nameTime
            else:
                dirName = str(nameTime + "_debug")
            subprocess.call(['mkdir', dirName])

            # Get all files in temp dir
            for file in os.listdir(fullPathTempDir):
                fullPathFile = fullPathTempDir + "/" + file
                if (not os.path.isdir(fullPathFile)):
                    subprocess.call(['cp', fullPathFile, dirName])

            # anomalousPath = root + '/' + 'anomalous_uls.csv'
            # warningPath = root + '/' + 'warning_uls.txt'
            # subprocess.call(['mv', anomalousPath, dirName])
            # subprocess.call(['mv', warningPath, dirName])
            # subprocess.call(['cp', paramOutput, dirName])

            shutil.make_archive(dirName, 'zip', root, dirName)
            zipName = dirName + ".zip"
            shutil.rmtree(dirName)  # delete debug directory
            subprocess.call(['mv', zipName, state_root + '/ULS_Database/'])
        except Exception as e:
            print('Error moving debug files:' + '\n')
            raise e

        # copy sqlite to where GUI can see it
        print("Copying sqlite file" + '\n')
        try:
            subprocess.call(['cp', outputSQL, state_root + '/ULS_Database/'])
        except Exception as e:
            print('Error copying ULS sqlite:' + '\n')
            raise e

        with open(root + '/data_files/lastSuccessfulRun.txt', 'w') as timeFile:
            timeFile.write(finishTime.isoformat())
    ###########################################################################

    return finishTime.isoformat()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='Process FS link data for AFC.')
    parser.add_argument('-i', '--interactive', action='store_true')
    parser.add_argument('-ca', '--combine_antenna_region',
                        action='store_true')
    parser.add_argument('-wfa', '--wfa', action='store_true')
    parser.add_argument('-unii_us', '--unii_us', default='5:7',
                        help='":" separated list of unii bands for US')

    parser.add_argument('-r', '--region', default='US:CA',
                        help='":" separated list of regions')

    args = parser.parse_args()
    interactive = args.interactive

    uniiStr = args.unii_us

    includeUnii5US = False
    includeUnii6US = False
    includeUnii7US = False
    includeUnii8US = False
    uniiList = uniiStr.split(':')
    for u in uniiList:
        if u == '5':
            includeUnii5US = True
        elif u == '6':
            includeUnii6US = True
        elif u == '7':
            includeUnii7US = True
        elif u == '8':
            includeUnii8US = True
        else:
            raise Exception('ERROR: Unrecognized unii band: ' + u)

    combineAntennaRegionFlag = args.combine_antenna_region
    wfaFlag = args.wfa

    print("Interactive = " + str(interactive))
    print("Include UNII-5 US = " + str(includeUnii5US))
    print("Include UNII-6 US = " + str(includeUnii6US))
    print("Include UNII-7 US = " + str(includeUnii7US))
    print("Include UNII-8 US = " + str(includeUnii8US))

    if not (includeUnii5US or includeUnii6US or includeUnii7US or includeUnii8US):
        raise Exception('ERROR: No UNII-Bands specified for US')

    print("Combine Antenna Region = " + str(combineAntennaRegionFlag))
    print("Region = " + args.region)
    print("WFA = " + str(wfaFlag))

    regionList = args.region.split(':')

    processUS = False
    processCA = False
    for r in regionList:
        if r == 'US':
            processUS = True
        elif r == 'CA':
            processCA = True
        else:
            raise Exception('ERROR: Unrecognized region: ' + r)

    print("Process US = " + str(processUS))
    print("Process CA = " + str(processCA))

    if not (processUS or processCA):
        raise Exception('ERROR: No regions specified')

    daily_uls_parse("/mnt/nfs/rat_transfer", interactive)
