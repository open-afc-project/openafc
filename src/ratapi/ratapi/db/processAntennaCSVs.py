import csv

def processAntFiles(inputDir, outputFile, logFile):
    logFile.write('Processing antenna files' + '\n')

    antennaModelDiamGainCsvFilePath    = inputDir + "/antenna_model_diameter_gain.csv"
    billboardReflectorCsvFilePath      = inputDir + "/billboard_reflector.csv"
    categoryB1AntennasCsvFilePath      = inputDir + "/category_b1_antennas.csv"
    highPerformanceAntennasCsvFilePath = inputDir + "/high_performance_antennas.csv"

    outputCsvFilePath = outputFile

    # Read in all input CSVs as dictionaries and fetch relevant columns
    # Billboard Reflector CSV
    billboardReflectorReader = csv.DictReader(open(billboardReflectorCsvFilePath, 'r'))

    reflectorAntennaModelList = []
    reflectorHeightList = []
    reflectorWidthList = []
    for col in billboardReflectorReader:
        reflectorAntennaModelList.append(col['antennaModel'])
        reflectorHeightList.append(col['height_m'])
        reflectorWidthList.append(col['width_m'])

    # Antenna Model/Diameter/Gain CSV
    antennaModelDiamGainReader = csv.DictReader(open(antennaModelDiamGainCsvFilePath, 'r'))

    standardModelList = []
    antennaModelList = []
    antennaDiameterList = []
    antennaGainList = []
    for col in antennaModelDiamGainReader:
        standardModelList.append(col['standardModel'])
        antennaModelList.append(col['antennaModel'])
        antennaDiameterList.append(col['diameter_m'])
        antennaGainList.append(col['gain_dBi'])

    # Category B1 Antenna CSV
    categoryB1AntennasReader = csv.DictReader(open(categoryB1AntennasCsvFilePath))
    antennaModelPrefixB1List = []
    for col in categoryB1AntennasReader:
        antennaModelPrefixB1List.append(col['antennaModelPrefix'])

    # HP Antenna CSV
    highPerformanceAntennasReader = csv.DictReader(open(highPerformanceAntennasCsvFilePath))
    antennaModelPrefixHpList = []
    for col in highPerformanceAntennasReader:
        antennaModelPrefixHpList.append(col['antennaModelPrefix'])

    # Make sure all antenna model characters are upper-case
    reflectorAntennaModelList = [antennaModel.upper() for antennaModel in reflectorAntennaModelList]
    standardModelList = [antennaModel.upper() for antennaModel in standardModelList]
    antennaModelList = [antennaModel.upper() for antennaModel in antennaModelList]
    antennaModelPrefixHpList = [antennaModelPrefix.upper() for antennaModelPrefix in antennaModelPrefixHpList]
    antennaModelPrefixB1List = [antennaModelPrefix.upper() for antennaModelPrefix in antennaModelPrefixB1List]

    # Open the output CSV
    csvWriter = csv.writer(open(outputCsvFilePath, 'w'), delimiter=',')
    # Write the header
    headerList = ['Ant Model','Type','Reflector Height (m)','Reflector Width (m)','Category','Diameter (m)','Midband Gain (dBi)']
    csvWriter.writerow(headerList)

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

        # Leave reflector height and reflector width columns empty for this type of antenna
        dataRow = [antennaModel, antType, '', '', category, diameter, midbandGain]
        csvWriter.writerow(dataRow)

    # Now process antenna models from the billboard_reflector CSV
    for ind, refAntennaModel in enumerate(reflectorAntennaModelList):
        # All antennas from the billboard_reflector CSV have type "Ref"
        antType = "Ref"

        # Check if the current antenna model starts with a B1 prefix
        category = "OTHER"
        if refAntennaModel.startswith(tuple(antennaModelPrefixB1List)):
            category = "B1"
        # Now check if the current antenna model starts with a HP prefix
        elif refAntennaModel.startswith(tuple(antennaModelPrefixHpList)):
            category = "HP"

        reflectorHeight = reflectorHeightList[ind]
        reflectorWidth = reflectorWidthList[ind]

        dataRow = [refAntennaModel, antType, reflectorHeight, reflectorWidth, category, '', '']
        csvWriter.writerow(dataRow)
