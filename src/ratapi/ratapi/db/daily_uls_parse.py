#!/usr/bin/env python
#
# Copyright (C) 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

import argparse
import datetime
import glob
import os
import shutil
import ssl
import subprocess

ssl._create_default_https_context = ssl.create_default_context

try:
    from .uls_downloader import (
        ULS_ZIP_MAX_BYTES,
        ULS_ZIP_MAX_RATIO,
        _MD5RedirectHandler,
        downloadFiles,
        extractZips,
        getMostRecentRegionDownload,
        handleRegionFailure,
        prepareAFCGitHubFiles,
        replaceRegionDataWithLastSuccess,
        safe_download,
    )
    from .uls_transformer import (
        dayMap,
        generateUlsScriptInputCA,
        generateUlsScriptInputStatic,
        generateUlsScriptInputUS,
        monthMap,
        neededFilesUS,
        processDailyFiles,
        readEntries,
        removeFromCombinedFile,
        storeDataIdentities,
        updateIndividualFile,
        verifyCountsFile,
        versionTime,
    )
    from .processAntennaCSVs import processAntFiles
    from .csvToSqliteULS import convertULS
    from .sort_callsigns_addfsid import sortCallsignsAddFSID
    from .fix_bps import fixBPS
    from .fix_params import fixParams
except ImportError:
    from uls_downloader import (
        ULS_ZIP_MAX_BYTES,
        ULS_ZIP_MAX_RATIO,
        _MD5RedirectHandler,
        downloadFiles,
        extractZips,
        getMostRecentRegionDownload,
        handleRegionFailure,
        prepareAFCGitHubFiles,
        replaceRegionDataWithLastSuccess,
        safe_download,
    )
    from uls_transformer import (
        dayMap,
        generateUlsScriptInputCA,
        generateUlsScriptInputStatic,
        generateUlsScriptInputUS,
        monthMap,
        neededFilesUS,
        processDailyFiles,
        readEntries,
        removeFromCombinedFile,
        storeDataIdentities,
        updateIndividualFile,
        verifyCountsFile,
        versionTime,
    )
    from processAntennaCSVs import processAntFiles
    from csvToSqliteULS import convertULS
    from sort_callsigns_addfsid import sortCallsignsAddFSID
    from fix_bps import fixBPS
    from fix_params import fixParams

# Global configurations / defaults
uniiStr = '5:7'
combineAntennaRegionFlag = False
wfaFlag = False
regionList = ['US', 'CA']
backupDir = None
processUS = True
processCA = True


def daily_uls_parse(state_root, interactive=False, uniiStr_param=None,
                    combineAntennaRegion_param=None, wfaFlag_param=None,
                    regionList_param=None, backupDir_param=None,
                    processCA_param=None):
    global uniiStr, combineAntennaRegionFlag, wfaFlag, regionList, backupDir, processCA
    if uniiStr_param is not None:
        uniiStr = uniiStr_param
    if combineAntennaRegion_param is not None:
        combineAntennaRegionFlag = combineAntennaRegion_param
    if wfaFlag_param is not None:
        wfaFlag = wfaFlag_param
    if regionList_param is not None:
        regionList = regionList_param
    if backupDir_param is not None:
        backupDir = backupDir_param
    if processCA_param is not None:
        processCA = processCA_param

    startTime = datetime.datetime.now()
    nameTime = startTime.isoformat().replace(":", '_')

    nameTime += "_UniiUS" + uniiStr.replace(":", "")

    temp = "/temp"
    save = "/country_history"

    didCAFail = False
    didUSFail = False

    root = state_root + "/daily_uls_parse"  # root so path is consistent

    ###########################################################################
    # If interactive, prompt to set root path                                 #
    ###########################################################################
    if interactive:
        print("Specify full path for root daily_uls_parse dir")
        value = input("Enter Directory (" + root + "): ")
        if value != "":
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
        if value != "":
            currentWeekday = int(value)
        if currentWeekday < 0 or currentWeekday > 6:
            print("ERROR: currentWeekday = " +
                  str(currentWeekday) + " invalid, must be in [0,6]")
            return
    ###########################################################################

    fullPathTempDir = root + temp
    fullPathSaveDir = root + save
    if backupDir is not None:
        fullPathSaveDir = backupDir

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
        if os.path.isdir(fullPathTempDir):
            try:
                shutil.rmtree(fullPathTempDir)  # delete temp folder
            except Exception as e:
                raise e
        # create temp directory to download files to
        os.mkdir(fullPathTempDir)
    ###########################################################################

    ###########################################################################
    # cd to temp dir and begin creating log file                              #
    ###########################################################################
    if not os.path.isdir(fullPathTempDir):
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
        try:
            #######################################################################
            # If interactive, prompt for downloading of data files for region     #
            #######################################################################
            if wfaFlag:
                downloadDataFilesFlag = False
            elif interactive:
                accepted = False
                while not accepted:
                    value = input("Download data files for " + region + "? (y/n): ")
                    if value == "y":
                        accepted = True
                        downloadDataFilesFlag = True
                    elif value == "n":
                        accepted = True
                        downloadDataFilesFlag = False
                    else:
                        print("ERROR: Invalid input: " + value + ", must be y or n")
            else:
                downloadDataFilesFlag = True
            #######################################################################

            #######################################################################
            # If downloadDataFilesFlag set, download data files for region        #
            #######################################################################
            if downloadDataFilesFlag:
                downloadFiles(region, logFile, currentWeekday, fullPathTempDir)
            #######################################################################

            regionDataDir = fullPathTempDir + '/' + region

            if region == 'US':
                ###################################################################
                # If interactive, prompt for extraction of files from zip files   #
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
                        print("ERROR: Invalid input: " + value + ", must be y or n")
                else:
                    extractZipFlag = True
                ###################################################################

                ###################################################################
                # If extractZipFlag set, extract files from zip files             #
                ###################################################################
                if extractZipFlag:
                    extractZips(logFile, regionDataDir)
                ###################################################################
        except Exception:
            if region == 'US':
                didUSFail = handleRegionFailure(region, didUSFail, fullPathSaveDir, fullPathTempDir)
            elif region == 'CA':
                didCAFail = handleRegionFailure(region, didCAFail, fullPathSaveDir, fullPathTempDir)

    ###########################################################################
    # If interactive, prompt for converting AFC GitHub data files             #
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
    # If interactive, prompt to set antennaPatternFileFile                    #
    ###########################################################################
    if interactive:
        if not processAntFilesFlag:
            flist = glob.glob(
                fullPathTempDir +
                "/afc_antenna_patterns_[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]T[0-9][0-9]_[0-9][0-9]_[0-9][0-9].[0-9][0-9][0-9][0-9][0-9][0-9].csv")  # noqa
            if len(flist):
                antennaPatternFileFile = os.path.basename(flist[-1])

        value = input("Enter Antenna Pattern filename (" +
                      antennaPatternFileFile + "): ")
        if value != "":
            antennaPatternFileFile = value
    ###########################################################################

    fullPathAntennaPatternFile = fullPathTempDir + "/" + antennaPatternFileFile

    ###########################################################################
    # If processAntFilesFlag set, process data files to create models         #
    ###########################################################################
    if processAntFilesFlag:
        try:
            processAntFiles(fullPathTempDir, processCA, combineAntennaRegionFlag,
                            fullPathTempDir + '/antenna_model_list.csv',
                            fullPathTempDir + '/antenna_prefix_list.csv',
                            fullPathAntennaPatternFile, logFile)
        except Exception:
            didCAFail = handleRegionFailure('CA', didCAFail, fullPathSaveDir, fullPathTempDir)
            processAntFiles(fullPathTempDir, processCA, combineAntennaRegionFlag,
                            fullPathTempDir + '/antenna_model_list.csv',
                            fullPathTempDir + '/antenna_prefix_list.csv',
                            fullPathAntennaPatternFile, logFile)
    ###########################################################################

    ###########################################################################
    # If interactive, prompt for processing download files for each region    #
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

    fullPathCoalitionScriptInput = fullPathTempDir + "/combined.txt"
    dataIdentities = {}

    ###########################################################################
    # If processDownloadFlag set, process Download files to create combined.txt#
    ###########################################################################
    if processDownloadFlag:
        with open(fullPathCoalitionScriptInput, 'w', encoding='utf8'):
            pass

        for region in regionList:
            regionDataDir = fullPathTempDir + '/' + region
            dataIdentity = None

            if region == 'US':
                try:
                    weeklyCreation = verifyCountsFile(regionDataDir + '/weekly')
                    uploadTime = processDailyFiles(
                        weeklyCreation, logFile, regionDataDir, currentWeekday)
                    dataIdentity = uploadTime.isoformat()

                    rasDataFileUSSrc = root + '/data_files/RASdatabase.dat'
                    rasDataFileUSTgt = regionDataDir + '/weekly/RA.dat_withDaily'
                    logFile.write("Copying " + rasDataFileUSSrc +
                                  ' to ' + rasDataFileUSTgt + '\n')
                    shutil.copy(rasDataFileUSSrc, rasDataFileUSTgt)

                    generateUlsScriptInputUS(
                        regionDataDir + '/weekly',
                        logFile,
                        fullPathCoalitionScriptInput)
                except Exception:
                    didUSFail = handleRegionFailure(region, didUSFail, fullPathSaveDir, fullPathTempDir)

                    weeklyCreation = verifyCountsFile(regionDataDir + '/weekly')
                    uploadTime = processDailyFiles(
                        weeklyCreation, logFile, regionDataDir, currentWeekday)
                    dataIdentity = uploadTime.isoformat()

                    rasDataFileUSSrc = root + '/data_files/RASdatabase.dat'
                    rasDataFileUSTgt = regionDataDir + '/weekly/RA.dat_withDaily'
                    logFile.write("Copying " + rasDataFileUSSrc +
                                  ' to ' + rasDataFileUSTgt + '\n')
                    shutil.copy(rasDataFileUSSrc, rasDataFileUSTgt)
                    generateUlsScriptInputUS(
                        regionDataDir + '/weekly',
                        logFile,
                        fullPathCoalitionScriptInput)

            elif region == 'CA':
                try:
                    dataIdentity = generateUlsScriptInputCA(
                        regionDataDir, logFile, fullPathCoalitionScriptInput)
                except Exception:
                    didCAFail = handleRegionFailure(region, didCAFail, fullPathSaveDir, fullPathTempDir)
                    dataIdentity = generateUlsScriptInputCA(
                        regionDataDir, logFile, fullPathCoalitionScriptInput)
            else:
                logFile.write('ERROR: Invalid region = ' + region)
                raise ValueError(f'Invalid region = {region}')
            assert dataIdentity is not None
            dataIdentities[region] = dataIdentity

        staticDataFile = root + '/data_files/static_fs_database.csv'
        if os.path.isfile(staticDataFile):
            generateUlsScriptInputStatic(staticDataFile, logFile, fullPathCoalitionScriptInput)
    ###########################################################################

    ###########################################################################
    # If interactive, prompt for running ULS Processor                        #
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

    coalitionScriptOutputFSFilename = 'FS_' + nameTime + '.csv'
    coalitionScriptOutputRASFilename = 'RAS_' + nameTime + '.csv'

    ###########################################################################
    # If interactive, prompt to set output file from ULS Processor            #
    ###########################################################################
    if interactive:
        if not runULSProcessorFlag:
            flist = glob.glob(
                fullPathTempDir +
                "/FS_[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]T[0-9][0-9]_[0-9][0-9]_[0-9][0-9].[0-9][0-9][0-9][0-9][0-9][0-9].csv")
            if len(flist):
                coalitionScriptOutputFSFilename = os.path.basename(flist[-1])
            flist = glob.glob(
                fullPathTempDir +
                "/RAS_[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]T[0-9][0-9]_[0-9][0-9]_[0-9][0-9].[0-9][0-9][0-9][0-9][0-9][0-9].csv")
            if len(flist):
                coalitionScriptOutputRASFilename = os.path.basename(flist[-1])

        value = input("Enter ULS Processor output FS filename (" +
                      coalitionScriptOutputFSFilename + "): ")
        if value != "":
            coalitionScriptOutputFSFilename = value

        value = input("Enter ULS Processor output RAS filename (" +
                      coalitionScriptOutputRASFilename + "): ")
        if value != "":
            coalitionScriptOutputRASFilename = value
    ###########################################################################

    fullPathCoalitionScriptOutput = fullPathTempDir + "/" + coalitionScriptOutputFSFilename
    fullPathRASDabataseFile = fullPathTempDir + "/" + coalitionScriptOutputRASFilename

    ###########################################################################
    # If runULSProcessorFlag set, run ULS processor                           #
    ###########################################################################
    if runULSProcessorFlag:
        mode = "proc_uls"
        if combineAntennaRegionFlag:
            mode += "_ca"

        logFile.write('Running through ULS processor\n')
        try:
            subprocess.call([
                root + '/uls-script',
                fullPathTempDir + '/combined.txt',
                fullPathCoalitionScriptOutput,
                fullPathRASDabataseFile,
                fullPathTempDir + '/antenna_model_list.csv',
                fullPathTempDir + '/antenna_prefix_list.csv',
                root + '/antenna_model_map.csv',
                fullPathTempDir + '/fcc_fixed_service_channelization.csv',
                fullPathTempDir + '/transmit_radio_unit_architecture.csv',
                uniiStr,
                mode
            ])
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

    sortedOutput = bpsScriptOutput.replace(".csv", "_sorted.csv")

    ###########################################################################
    # If runSortCallsignsAddFSIDFlag set, run sortCallsignsAddFSID            #
    ###########################################################################
    if runSortCallsignsAddFSIDFlag:
        fsidTableFile = root + '/data_files/fsid_table.csv'
        fsidTableBakFile = root + '/data_files/fsid_table_bak_' + nameTime + '.csv'
        logFile.write("Backing up FSID table for to: " +
                      fsidTableBakFile + '\n')
        shutil.copy(fsidTableFile, fsidTableBakFile)
        logFile.write("Running through sort callsigns add FSID script\n")
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

    paramOutput = sortedOutput.replace(".csv", "_param.csv")

    ###########################################################################
    # If runFixParamsFlag set, run fixParams                                  #
    ###########################################################################
    if runFixParamsFlag:
        logFile.write("Running fixParams\n")
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
                  str(timeDiff.total_seconds()) + ' seconds\n')
    logFile.close()
    ###########################################################################

    os.chdir(root)

    ###########################################################################
    # If not interactive:                                                     #
    # * create zip file containing intermediate file for debugging            #
    # * copy sqlite file to ULS_Database directory for use by afc-engine      #
    ###########################################################################
    if not interactive:
        print("Creating and moving debug files\n")
        try:
            if wfaFlag:
                dirName = "WFA_testvector_FS_" + nameTime
            else:
                dirName = str(nameTime + "_debug")
            os.mkdir(dirName)

            for file in os.listdir(fullPathTempDir):
                fullPathFile = fullPathTempDir + "/" + file
                if not os.path.isdir(fullPathFile):
                    shutil.copy(fullPathFile, dirName)

            shutil.make_archive(dirName, 'zip', root, dirName)
            zipName = dirName + ".zip"
            shutil.rmtree(dirName)
            shutil.move(zipName, state_root + '/ULS_Database/')
        except Exception as e:
            print('Error moving debug files:\n')
            raise e

        print("Copying sqlite file\n")
        try:
            shutil.copy(outputSQL, state_root + '/ULS_Database/')
        except Exception as e:
            print('Error copying ULS sqlite:\n')
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
    parser.add_argument('-s_dir', '--save_dir', default=None,
                        help='Location of the saves')

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

    backupDir = None
    if args.save_dir is not None:
        backupDir = str(args.save_dir)
        print("Backups are expected to be located at " + str(backupDir))

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
