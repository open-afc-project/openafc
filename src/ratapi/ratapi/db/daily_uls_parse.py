import os, urllib, datetime, zipfile, shutil, subprocess, sys
from collections import OrderedDict
import ssl
from ..db.csvToSqliteULS import convertULS
from sort_callsigns_all import sortCallsigns
from fix_bps import fixBPS

ssl._create_default_https_context = ssl._create_unverified_context

currentWeekday = datetime.datetime.today().weekday() # weekday() is 0 indexed at monday
# file types we need to consider along with their # of | symbols (i.e. # of cols - 1)
neededFiles = {'AN.dat': 37, 'CP.dat': 13, 'EM.dat': 15, 'EN.dat': 29, 'FR.dat': 29, 'HD.dat': 58, 'LO.dat': 50, 'PA.dat': 21, 'SG.dat': 14}


# map to reuse weekday in loops
dayMap = OrderedDict() # ordered dictionary so order is mainatined 
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

# Downloads the weekly file and the daily file(s) up until the most recent file
def downloadFiles(logFile):
    # download the latest Weekly Update
    weeklyURL =  'https://data.fcc.gov/download/pub/uls/complete/l_micro.zip'
    logFile.write('Downloading weekly' + '\n')
    urllib.urlretrieve(weeklyURL, 'weekly.zip')

    # download all the daily updates starting from Sunday up to that day
    # example: on Wednesday morning, we will download the weekly update PLUS Sun, Mon and Tue daily updates
    for key, day in dayMap.items():
        if (currentWeekday == 6):
            continue # this is LAST Sunday's files, so we skip this entry 
        dayStr = day
        dailyURL =  'https://data.fcc.gov/download/pub/uls/daily/l_mw_' + dayStr +'.zip'
        logFile.write('Downloading ' + dayStr + '\n')
        urllib.urlretrieve(dailyURL, dayStr + '.zip')
        # Exit after processing yesterdays file
        if (key == currentWeekday - 1 or (currentWeekday == 0 and key == 6)):
            break

# Extracts all the zip files into sub-directories
def extractZips(logFile):
    logFile.write('Extracting zips' + '\n')
    # unzip into sub-directories of temp directory
    for tempZip in os.listdir(os.getcwd()): # unzip every zip
        if (tempZip.endswith('.zip')):
            logFile.write('Extracting ' + tempZip + '\n')
            fileName = os.path.abspath(tempZip) # get full path
            zip_file = zipfile.ZipFile(fileName) # zip object
            subDirName = tempZip.replace('.zip', '')
            os.mkdir(subDirName) # sub-directory to extract to 
            zip_file.extractall(subDirName) 
            zip_file.close() 

# Returns the datetime object based on the counts file
def verifyCountsFile(dirName, root, temp):
    os.chdir(root + temp + "/" + dirName)
    # check weekly date 
    with open('counts', 'r') as countsFile: 
        line = countsFile.readline()
        # FCC Format for the line is:
        # File Creation Date: Sun Oct  3 17:59:04 EDT 2021
        dateStr = line.replace('File Creation Date: ', '') # remove non-date part of string
        dateData = dateStr.split() # split into word array
        weekday = dateData[0] 
        month = monthMap.get(dateData[1].lower(), 'err' ) # convert month string to an int between 1 and 12
        day = int(dateData[2]) # day of the month as a Number
        time = dateData[3]
        timeZone = dateData[4]
        year = int(dateData[5])
        timeData = map(lambda string : int(string)  , time.split(':')) # convert string time to numbers array
        hours = timeData[0]
        mins = timeData[1]
        sec = timeData[2]
        if(month != 'err'):
            os.chdir(root + temp) # change back to temp directory
            fileCreationDate = datetime.datetime(year, month, day, hours, mins, sec)
            return fileCreationDate
        else: 
            raise Exception('ERROR: Could not parse month of FCC string in counts file for ' + dirName + ' update')
            

# Removes any record with the given id from the given file
def removeFromCombinedFile(fileName, ids_to_remove):
    weeklyAndDailyPath = '../weekly/' + fileName + '_withDaily'
    # if the weekly + daily data exists use that, otherwise make the file
    if(os.path.isfile(weeklyAndDailyPath)):
        # open new file that contains weekly and daily
        with open(weeklyAndDailyPath + '_temp', 'w') as withDaily: 
            # open older file 
            with open(weeklyAndDailyPath , 'r' ) as weekly:
                for line in weekly: 
                    cols = line.split('|')
                    # only write when the id is not in the list of ids 
                    if(not cols[1] in ids_to_remove):
                        withDaily.write(line)
        # remove old combined, move new one to right place
        os.remove(weeklyAndDailyPath)
        os.rename(weeklyAndDailyPath + '_temp', weeklyAndDailyPath)
    else:
        # create file that contains weekly and daily
        with open(weeklyAndDailyPath, 'w') as withDaily: 
            # open weekly file
            with open('../weekly/' + fileName , 'r' ) as weekly:
                record = '' 
                symbolCount = 0
                numExpectedCols = neededFiles[fileName]
                for line in weekly:
                    # remove newline characters from line
                    line = line.replace('\n', '')
                    line = line.replace('\r', '')
                    # the line was just newline character(s), skip it
                    if(line == '' or line == ' '):
                        continue
                    # the line is a single piece of data that does not contain the | character
                    elif(not '|' in line):
                        record += line
                        continue 
                    else: 
                        symbolCount += line.count('|') # this many | were in the line
                        record += line  
                    # the record is complete if the number of | symbols is equal to the number of expected cols
                    if (symbolCount == numExpectedCols): 
                        cols = record.split('|')
                        fileType = cols[0]
                        # Ensure we need this entry
                        if(fileType + ".dat" in neededFiles.keys()):
                            # only write when the id is not in the list of ids 
                            if(not cols[1] in ids_to_remove):
                                record += "\r\n" # add newline
                                withDaily.write(record)
                            #reset for the next record 
                            record = ''
                            symbolCount = 0
                    elif (symbolCount > numExpectedCols):
                        e = Exception('ERROR: Could not process record. More columns than expected in weekly file')
                        raise e


# Update specific datafile with daily data
def updateIndividualFile(fileName, lineBuffer):
    weeklyAndDailyPath = '../weekly/' + fileName + '_withDaily'
    if(os.path.isfile(weeklyAndDailyPath)):
        # open file that contains weekly and daily
        with open(weeklyAndDailyPath, 'a') as withDaily:       
            withDaily.write(lineBuffer)
    else:
        e =  Exception('Combined file ' + weeklyAndDailyPath + ' does not exist')
        raise e 

# Reads the file and creates well formed entries from FCC data. 
# The ONLY thing that consititutes a valid entry is the number of | characters (e.g. AN files have 38 columns and thus 37 | per record)
def readEntries(fileType):
    recordBuffer = '' # buffer to limit number of file open() calls
    idsToRemove = []
    with open(fileType) as infile:
        numExpectedCols = neededFiles[fileType]
        record = ''
        symbolCount = 0   
        # Iterate over the lines in the file
        for line in infile:
            # remove newline characters from line
            line = line.replace('\n', '')
            line = line.replace('\r', '')
            
            # the line we were given was just newline character(s) or whitespace, skip it
            if(line == '' or line == ' '):
                continue
            # the line is a single piece of data that does not contain the | character
            elif(not '|' in line):
                record += line
                continue 
            else: 
                symbolCount += line.count('|') # this many | were in the line
                record += line  
            # the record is complete if the number of | symbols is equal to the number of expected cols
            if(symbolCount == numExpectedCols):
                cols = record.split('|')
                fccId = cols[1] # FCC unique system identifier should always be this index
                # only need to remove an ID once per file per day
                if(not fccId in idsToRemove): 
                    idsToRemove.append(fccId)
                recordBuffer += record + '\r\n' # store the record to the buffer with proper newline for csv format  
                record = '' #reset the record 
                symbolCount = 0
            elif (symbolCount > numExpectedCols):
                raise Exception('ERROR: Could not process record more columns than expected')
    removeFromCombinedFile(fileType, idsToRemove)
    updateIndividualFile(fileType, recordBuffer)

# Processes the daily files, replacing weekly entries when needed 
def processDailyFiles(weeklyCreation, root, logFile, temp):
    logFile.write('Processing daily files' + '\n')
    weeklyFormatFixed = False
    for key, day in dayMap.items():
        if (currentWeekday == 6):
            continue # this is LAST Sunday's files, so we skip this entry
        # ensure counts file is newer than weekly
        fileCreationDate = verifyCountsFile(day, root, temp)
        timeDiff = fileCreationDate - weeklyCreation
        if(timeDiff.total_seconds() > 0):
            os.chdir(root + temp + '/' + day) # change into the folder for the day, starting with sunday 
            for dailyFile in os.listdir(os.getcwd()):
                if (dailyFile in neededFiles.keys()):
                    logFile.write('Processing ' + dailyFile + ' for: ' + day + '\n')
                    readEntries(dailyFile)
                    weeklyFormatFixed = True
            # Exit after processing yesterdays file
            if (key == currentWeekday - 1 or (currentWeekday == 0 and key == 6)):
                break
            os.chdir(root + temp) # change back to temp
        else:
            logFile.write('INFO: Skipping ' + day + ' files because they are older than the weekly file' + '\n')
    # in this case there were no daily files processed so we need to fix weekly data 
    # normally weekly data formatting is corrected when adding daily data 
    if (not weeklyFormatFixed):
        for file in neededFiles.keys():
            # removeFromCombinedFile() will fix any formatting in FCC data
            # passing an empty list for second arg means no records will be removed 
            removeFromCombinedFile(file, []) 

# Generates the combined text file that the coalition processor uses. 
def generateUlsScriptInput(root, logFile, temp):
    logFile.write('Generating combined.txt for uls script' + '\n')
    os.chdir(root + temp)
    with open('combined.txt', 'w') as combined:
        for weeklyFile in os.listdir('weekly'):
            if "withDaily" in weeklyFile:
                logFile.write('Adding ' + weeklyFile + " to combined.txt" + '\n')
                with open(root +  temp +'/weekly/' + weeklyFile, 'r') as infile:
                    for line in infile:
                        combined.write(line)
    os.chdir(root)

def daily_uls_parse(state_root, isManual = False):
    startTime = datetime.datetime.now()
    root = state_root + "/daily_uls_parse"# root so path is consisent
    temp = '/temp'
    if isManual:
        temp = '/temp_manual'
    # Ensure temp dir does not exist 
    if (os.path.isdir(root + temp)):
        try:
            shutil.rmtree(root + temp) #delete temp folder
        except Exception as e: 
            # LOGGER.error('ERROR: Could not delete old temp directory:')
            raise e
    # create temp directory to download files to
    os.mkdir(root + temp)
    os.chdir(root + temp) #change to temp 
    logname = root + temp + "/dailyParse_" + startTime.isoformat() + ".log"
    logFile = open(logname, 'w')
    logFile.write('Starting update at: ' + startTime.isoformat() + '\n')


    # download and unzip
    downloadFiles(logFile)
    extractZips(logFile)


    # get the time creation of weekly file from the counts file
    weeklyCreation = verifyCountsFile('weekly', root, temp)
    # process the daily files day by day
    processDailyFiles(weeklyCreation, root, logFile, temp)

    os.chdir(root) # change back to root of this script
    # generate the combined csv/txt file for the coalition uls processor 
    generateUlsScriptInput(root, logFile, temp) 

    # run through the uls processor 
    logFile.write('Running through ULS processor' + '\n')
    nameTime =  datetime.datetime.now().isoformat().replace(":", '_')
    coalitionScriptOutput = root + temp +'/CONUS_ULS ' + nameTime + '.csv'
    try:
        subprocess.call(['./uls-script', root + temp + '/combined.txt', coalitionScriptOutput]) 
    except Exception as e: 
        logFile.write('ERROR: ULS processor error:')
        raise e
    
    # run fixBPS script 
    bpsScriptOutput = coalitionScriptOutput.replace('.csv', '_fixedBPS.csv')
    logFile.write("Running through BPS script" + '\n')
    fixBPS(coalitionScriptOutput, bpsScriptOutput)
    
    # run sort callsign script
    sortedOutput = bpsScriptOutput.replace(".csv", "_sorted.csv")
    logFile.write("Running through sort callsigns script" + '\n')
    sortCallsigns(bpsScriptOutput, sortedOutput)
    # convert uls csv to sqlite   
    updated, inserted = convertULS(sortedOutput, state_root, logFile)
    

    # Give sqlite file proper permissions
    outputSQL = sortedOutput.replace('.csv', '.sqlite3')
    # logFile.write("Updating permissions for sqlite file" ) 
    # try:
    #     subprocess.call(['chown', 'fbrat', outputSQL]) 
    #     subprocess.call(['chgrp', 'fbrat', outputSQL]) 
    #     subprocess.call(['chmod', '750', outputSQL]) 
    # except Exception as e: 
    #     logFile.write('ERROR: Could not set permissions for sqlite file')
    #     raise e

   
    logFile.write("Creating and moving debug files")
    # create debug zip containing final csv and anomalous uls and move it to where GUI can see
    try:
        dirName = str(nameTime + "_debug")
        subprocess.call(['mkdir', dirName]) 
        anomalousPath = root + '/' + 'anomalous_uls.csv'
        subprocess.call(['mv', anomalousPath, dirName]) 
        subprocess.call(['mv', sortedOutput, dirName]) 
        shutil.make_archive( dirName , 'zip', dirName)
        zipName = dirName + ".zip"
        shutil.rmtree(dirName) #delete debug directory
    except Exception as e: 
        logFile.write('Error moving debug files:' + '\n')
        raise e


    logFile.write("Updating yesterdays DB" + '\n')
    # replace the yesterday db file with the one generated today, for tomorrow
    try:
        subprocess.call(['rm', root + '/data_files/yesterdayDB.sqlite3']) 
        subprocess.call(['cp', outputSQL, root + "/data_files/yesterdayDB.sqlite3"]) 
        # subprocess.call(['chown', 'fbrat', zipName]) 
        # subprocess.call(['chgrp', 'fbrat', zipName]) 
        # subprocess.call(['chmod', '750', zipName]) 
        subprocess.call(['mv', zipName,  state_root + '/ULS_Database/']) 
    except Exception as e: 
        logFile.write('Error moving debug files:' + '\n')
        raise e
    
    # move sqlite to where GUI can see it
    logFile.write("Moving sqlite file" + '\n')
    try:
        subprocess.call(['mv', outputSQL, state_root + '/ULS_Database/']) 
    except Exception as e: 
        logFile.write('Error moving ULS sqlite:' + '\n')
        raise e
    

    # try:
    #     shutil.rmtree(root + '/temp') #delete temp folder
    # except Exception as e: 
    #     logFile.write('Error deleting temp directory:', e)


    finishTime = datetime.datetime.now()
    with open(root + '/data_files/lastSuccessfulRun.txt', 'w') as timeFile:
        timeFile.write(finishTime.isoformat())
    logFile.write('Update finished at: ' + finishTime.isoformat() + '\n')
    timeDiff = finishTime - startTime
    logFile.write('Update took ' + str(timeDiff.total_seconds()) + ' seconds' + '\n')
    logFile.close()
    return updated, inserted, finishTime.isoformat()
