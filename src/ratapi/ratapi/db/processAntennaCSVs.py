import csv


def fixModelName(modelPrefix, modelName):
    ##########################################################################
    # Convert modelName to uppercase
    ##########################################################################
    modelName = modelName.upper()
    ##########################################################################

    ##########################################################################
    # Remove non-alhpanumeric characters
    ##########################################################################
    modelName = ''.join(filter(str.isalnum, modelName))
    ##########################################################################

    ##########################################################################
    # Prepend prefix
    ##########################################################################
    modelName = modelPrefix + modelName
    ##########################################################################

    return modelName


def processAntFiles(
        inputDir,
        processCA,
        combineAntennaRegionFlag,
        antennaListFile,
        antennaPrefixFile,
        antennaPatternFile,
        logFile):
    logFile.write('Processing antenna files' + '\n')

    antennaModelDiamGainCsvFilePath = inputDir + "/antenna_model_diameter_gain.csv"
    billboardReflectorCsvFilePath = inputDir + "/billboard_reflector.csv"
    categoryB1AntennasCsvFilePath = inputDir + "/category_b1_antennas.csv"
    highPerformanceAntennasCsvFilePath = inputDir + "/high_performance_antennas.csv"

    if processCA:
        antennaPatternCsvFilePath = inputDir + "/CA/AP.csv"

    if combineAntennaRegionFlag:
        prefixUS = ""
        prefixCA = ""
    else:
        prefixUS = "US:"
        prefixCA = "CA:"

    antennaListFilePath = antennaListFile

    # Read in all input CSVs as dictionaries and fetch relevant columns

    ##########################################################################
    # Billboard Reflector CSV
    ##########################################################################
    billboardReflectorReader = csv.DictReader(
        open(billboardReflectorCsvFilePath, 'r'))

    reflectorAntennaModelList = []
    reflectorHeightList = []
    reflectorWidthList = []
    for row in billboardReflectorReader:
        reflectorAntennaModelList.append(
            fixModelName(prefixUS, row['antennaModel']))
        reflectorHeightList.append(row['height_m'])
        reflectorWidthList.append(row['width_m'])
    ##########################################################################

    ##########################################################################
    # Antenna Model/Diameter/Gain CSV
    ##########################################################################
    antennaModelDiamGainReader = csv.DictReader(
        open(antennaModelDiamGainCsvFilePath, 'r'))

    standardModelList = []
    antennaDiameterList = []
    antennaGainList = []
    for row in antennaModelDiamGainReader:
        standardModelList.append(fixModelName(prefixUS, row['standardModel']))
        antennaDiameterList.append(row['diameter_m'])
        antennaGainList.append(row['gain_dBi'])
    ##########################################################################

    ##########################################################################
    # Category B1 Antenna CSV
    ##########################################################################
    categoryB1AntennasReader = csv.DictReader(
        open(categoryB1AntennasCsvFilePath))
    antennaModelPrefixB1List = []
    for row in categoryB1AntennasReader:
        antennaModelPrefixB1List.append(
            fixModelName(prefixUS, row['antennaModelPrefix']))
    ##########################################################################

    ##########################################################################
    # HP Antenna CSV
    ##########################################################################
    highPerformanceAntennasReader = csv.DictReader(
        open(highPerformanceAntennasCsvFilePath))
    antennaModelPrefixHpList = []
    for row in highPerformanceAntennasReader:
        antennaModelPrefixHpList.append(
            fixModelName(prefixUS, row['antennaModelPrefix']))
    ##########################################################################

    ##########################################################################
    # Antenna Pattern File from ISED (Canada)
    ##########################################################################
    antennaPatternModelList = []
    antennaPatternGainList = []
    antennaPatternDiameterList = []
    antennaPatternTypeList = []
    antennaPatternAzimuthList = []
    antennaPatternAttenuationList = []
    if processCA:
        antennaPatternReader = csv.DictReader(open(antennaPatternCsvFilePath))
        for row in antennaPatternReader:
            antennaPatternModelList.append(fixModelName(
                prefixCA, row['Antenna Model Number']))
            antennaPatternGainList.append(row['Antenna Gain [dBi]'])
            antennaPatternDiameterList.append(row['Antenna Diameter'])
            antennaPatternTypeList.append(row['Pattern Type'])
            antennaPatternAzimuthList.append(row['Pattern Azimuth [deg]'])
            antennaPatternAttenuationList.append(
                row['Pattern Attenuation [dB]'])
    ##########################################################################

    antmap = {}
    antpatternRaw = {}
    antpatternInterp = {}

    # First process antenna models from the antenna_model_diameter_gain CSV
    for ind, antennaModel in enumerate(standardModelList):
        # All antennas from the antenna_model_diameter_gain CSV have type "Ant"
        antType = "Ant"

        # Check if the current antenna model starts with a B1 prefix
        category = "OTHER"
        if antennaModel.startswith(tuple(antennaModelPrefixB1List)):
            category = "B1"
        # Now check if the current antenna model starts with a HP prefix
        elif antennaModel.startswith(tuple(antennaModelPrefixHpList)):
            category = "HP"

        # These lists are both in sync with the antennaModel variable
        diameter = antennaDiameterList[ind]
        midbandGain = antennaGainList[ind]

        if not diameter:
            # print("Current antenna (model name = {}) is empty".format(antennaModel, diameter))
            diameter = ''
        if not midbandGain:
            # print("Current antenna (model name = {}) is empty".format(antennaModel, midbandGain))
            midbandGain = ''

        if antennaModel in antmap:
            r = antmap[antennaModel]
            if (r['Diameter (m)'] != diameter) or (
                    r['Midband Gain (dBi)'] != midbandGain):
                logFile.write(
                    'WARNING: Antenna model ' +
                    antennaModel +
                    ' defined multiple times in file ' +
                    antennaModelDiamGainCsvFilePath +
                    ' with different parameters\n')
        else:
            row = {}
            row['Ant Model'] = antennaModel
            row['Type'] = antType
            row['Reflector Height (m)'] = ''
            row['Reflector Width (m)'] = ''
            row['Category'] = category
            row['Diameter (m)'] = diameter
            row['Midband Gain (dBi)'] = midbandGain
            row['Has Pattern'] = '0'

            antmap[antennaModel] = row

    # Now process antenna models from the billboard_reflector CSV
    for ind, antennaModel in enumerate(reflectorAntennaModelList):
        # All antennas from the billboard_reflector CSV have type "Ref"
        antType = "Ref"

        # Check if the current antenna model starts with a B1 prefix
        category = "OTHER"
        if antennaModel.startswith(tuple(antennaModelPrefixB1List)):
            category = "B1"
        # Now check if the current antenna model starts with a HP prefix
        elif antennaModel.startswith(tuple(antennaModelPrefixHpList)):
            category = "HP"

        reflectorHeight = reflectorHeightList[ind]
        reflectorWidth = reflectorWidthList[ind]

        if antennaModel in antmap:
            r = antmap[antennaModel]
            if (r['Reflector Height (m)'] != reflectorHeight) or (
                    r['Reflector Width (m)'] != reflectorWidth):
                logFile.write(
                    'WARNING: Antenna model ' +
                    antennaModel +
                    ' defined multiple times in file ' +
                    billboardReflectorCsvFilePath +
                    ' with different parameters\n')
        else:
            row = {}
            row['Ant Model'] = antennaModel
            row['Type'] = antType
            row['Reflector Height (m)'] = reflectorHeight
            row['Reflector Width (m)'] = reflectorWidth
            row['Category'] = category
            row['Diameter (m)'] = ''
            row['Midband Gain (dBi)'] = ''
            row['Has Pattern'] = '0'

            antmap[antennaModel] = row

    # Now process antenna models from the antenna_pattern CSV
    for ind, antennaModel in enumerate(antennaPatternModelList):
        # All antennas from the antenna_pattern CSV have type "Ant"
        antType = "Ant"

        antennaGain = antennaPatternGainList[ind]
        diameter = antennaPatternDiameterList[ind]
        type = antennaPatternTypeList[ind]
        azimuth = float(antennaPatternAzimuthList[ind])
        attenuation = float(antennaPatternAttenuationList[ind])

        if type == "HH":

            if antennaModel in antmap:
                row = antmap[antennaModel]
                if (antmap[antennaModel]['Diameter (m)'] != diameter):
                    logFile.write(
                        'WARNING: Antenna model ' +
                        antennaModel +
                        ' multiple diameters specified ' +
                        diameter +
                        ' and ' +
                        antmap[antennaModel]['Diameter (m)'] +
                        '\n')
                    antmap[antennaModel]['Diameter (m)'] = diameter
                if (antmap[antennaModel]['Midband Gain (dBi)'] != antennaGain):
                    logFile.write(
                        'WARNING: Antenna model ' +
                        antennaModel +
                        ' multiple gain values specified ' +
                        antennaGain +
                        ' and ' +
                        antmap[antennaModel]['Midband Gain (dBi)'] +
                        '\n')
                    antmap[antennaModel]['Midband Gain (dBi)'] = antennaGain
            else:
                row = {}
                row['Ant Model'] = antennaModel
                row['Type'] = antType
                row['Reflector Height (m)'] = ''
                row['Reflector Width (m)'] = ''
                row['Category'] = 'OTHER'
                row['Diameter (m)'] = diameter
                row['Midband Gain (dBi)'] = antennaGain
                antmap[antennaModel] = row
            row['Has Pattern'] = '1'

            ###################################################################
            # Compute Angle Off Boresight (aob) in [0, 180]                    #
            ###################################################################
            aob = azimuth
            while aob >= 180.0:
                aob = aob - 360.0
            while aob < -180.0:
                aob = aob + 360.0
            aob = abs(aob)
            ###################################################################

            if antennaModel in antpatternRaw:
                antpatternRaw[antennaModel].append(tuple([aob, attenuation]))
            else:
                antpatternRaw[antennaModel] = [tuple([aob, attenuation])]

    ##########################################################################
    # Write antennaListFile (antenna_model_list.csv)                           #
    ##########################################################################
    with open(antennaListFilePath, 'w') as fout:
        headerList = [
            'Ant Model',
            'Type',
            'Reflector Height (m)',
            'Reflector Width (m)',
            'Category',
            'Diameter (m)',
            'Midband Gain (dBi)',
            'Has Pattern']

        csvwriter = csv.writer(fout, delimiter=',')
        csvwriter = csv.DictWriter(fout, headerList)
        csvwriter.writeheader()

        for antennaModel in sorted(antmap.keys()):
            row = antmap[antennaModel]
            csvwriter.writerow(row)
    ##########################################################################

    ##########################################################################
    # Write antennaPrefixFile (antenna_prefix_list.csv)                        #
    ##########################################################################
    with open(antennaPrefixFile, 'w') as fout:
        headerList = ['Prefix', 'Type', 'Category']

        csvwriter = csv.writer(fout, delimiter=',')
        csvwriter = csv.DictWriter(fout, headerList)
        csvwriter.writeheader()

        row = {}
        for prefix in antennaModelPrefixB1List:
            row['Prefix'] = prefix
            row['Type'] = 'Ant'
            row['Category'] = 'B1'
            csvwriter.writerow(row)

        for prefix in antennaModelPrefixHpList:
            row['Prefix'] = prefix
            row['Type'] = 'Ant'
            row['Category'] = 'HP'
            csvwriter.writerow(row)

    ##########################################################################

    aobStep = 0.1
    aobStart = 0.0
    aobStop = 180.0
    numAOB = int(round((aobStop - aobStart) / aobStep)) + 1

    for antennaModel in sorted(antpatternRaw.keys()):
        antpatternRaw[antennaModel].sort(key=lambda y: y[0])

        #######################################################################
        # For Angle Off Boresight values specified more than once,             #
        # use min attenuation (largest gain)                                   #
        #######################################################################
        i = 0
        while i <= len(antpatternRaw[antennaModel]) - 2:
            if antpatternRaw[antennaModel][i][0] == antpatternRaw[antennaModel][i + 1][0]:
                if antpatternRaw[antennaModel][i][1] != antpatternRaw[antennaModel][i + 1][1]:
                    logFile.write(
                        'WARNING: Antenna model ' +
                        antennaModel +
                        ' angle off boresight ' +
                        antpatternRaw[antennaModel][i][0] +
                        ' has multiple attenuations specified\n')
                antpatternRaw[antennaModel][i][1] == min(
                    antpatternRaw[antennaModel][i][1], antpatternRaw[antennaModel][i + 1][1])
                antpatternRaw[antennaModel].pop(i + 1)
            else:
                i = i + 1
        #######################################################################

        N = len(antpatternRaw[antennaModel])
        minAOB = antpatternRaw[antennaModel][0][0]
        maxAOB = antpatternRaw[antennaModel][N - 1][0]
        useFlag = True

        #######################################################################
        # Confirm that there is data over the entire range [0,180]             #
        #######################################################################
        if (minAOB > 0.0) or (maxAOB < 180.0):
            useFlag = False
            logFile.write(
                'WARNING: Antenna model ' +
                antennaModel +
                ' gain values do not cover the angle off boresight range [0,180], range is [' +
                minAOB +
                ',' +
                maxAOB +
                ']\n')
        #######################################################################

        #######################################################################
        # Interpolate pattern data to cover [0,180] in 0.1 deg steps           #
        #######################################################################
        if useFlag:
            k = 0
            for i in range(numAOB):
                aob = (aobStart * (numAOB - 1 - i) +
                       aobStop * i) / (numAOB - 1)
                while antpatternRaw[antennaModel][k][0] < aob:
                    k += 1
                if k == 0:
                    attn = antpatternRaw[antennaModel][0][1]
                else:
                    k1 = k
                    k0 = k - 1
                    aob0 = antpatternRaw[antennaModel][k0][0]
                    aob1 = antpatternRaw[antennaModel][k1][0]
                    attn0 = antpatternRaw[antennaModel][k0][1]
                    attn1 = antpatternRaw[antennaModel][k1][1]
                    attn = (attn0 * (aob1 - aob) + attn1 *
                            (aob - aob0)) / (aob1 - aob0)
                if antennaModel in antpatternInterp:
                    antpatternInterp[antennaModel].append(attn)
                else:
                    antpatternInterp[antennaModel] = [attn]
        #######################################################################

    antModelList = sorted(antpatternInterp.keys())
    with open(antennaPatternFile, 'w') as fout:
        # Open the output CSV
        headerList = ['Off-axis angle (deg)'] + antModelList

        csvwriter = csv.writer(fout, delimiter=',')
        csvwriter = csv.DictWriter(fout, headerList)
        csvwriter.writeheader()

        for i in range(numAOB):
            aob = (aobStart * (numAOB - 1 - i) + aobStop * i) / (numAOB - 1)
            row.clear()
            row['Off-axis angle (deg)'] = str(aob)

            for antennaModel in antModelList:
                row[antennaModel] = str(-1.0 *
                                        antpatternInterp[antennaModel][i])
            csvwriter.writerow(row)
