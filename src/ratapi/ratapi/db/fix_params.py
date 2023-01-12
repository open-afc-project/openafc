import csv
import sys
import math
from os.path import exists

csmap = {}
c  = 2.99792458e8
unii5StartFreqMHz = 5925
unii5StopFreqMHz  = 6425
unii6StartFreqMHz = 6425
unii6StopFreqMHz  = 6525
unii7StartFreqMHz = 6525
unii7StopFreqMHz  = 6875
unii8StartFreqMHz = 6875
unii8StopFreqMHz  = 7125

typicalReflectorDimensions = [
    (1.83,2.44),
    (2.44,3.05),
    (2.44,3.66),
    (3.05,4.57),
    (3.05,4.88),
    (3.05,7.32),
    (3.66,4.88),
    (4.27,4.88),
    (4.88,6.10),
    (4.88,7.32),
    (6.10,7.32),
    (6.10,9.14),
    (6.10,9.75),
    (7.32,9.14),
    (9.14,9.75),
    (9.14,12.19),
    (9.14,14.63),
    (12.19,15.24) ]

def isTypicalReflectorDimension(height, width):
    found = False
    for refDim in typicalReflectorDimensions:
        typicalHeight = refDim[0]
        typicalWidth = refDim[1]
        if (abs(typicalHeight - height) <= 0.1) and (abs(typicalWidth - width) <= 0.1):
            found = True
            break
    return found

def fixParams(inputPath, outputPath, logFile, backwardCompatiblePR):
    logFile.write('Fixing parameters' + '\n')

    file_handle = open(inputPath, 'r')
    csvreader = csv.DictReader(file_handle)

    fieldnames = csvreader.fieldnames + [
        'return_FSID',
         'Rx Gain (dBi)',
         'Rx Height to Center RAAT (m)',
         'Rx Diversity Gain (dBi)',
         'Rx Diversity Height to Center RAAT (m)',
         'Tx Gain (dBi)',
         'Tx Height to Center RAAT (m)',
         'Rx Near Field Ant Diameter (m)',
         'Rx Near Field Dist Limit (m)',
         'Rx Near Field Ant Efficiency'
    ]

    maxNumPR = 0
    for field in csvreader.fieldnames:
        wordList = field.split()
        if (wordList[0] == 'Passive') and (wordList[1] == 'Repeater'):
            n = int(wordList[2])
            if (n > maxNumPR):
                maxNumPR = n
    print('maxNumPR = {}'.format(str(maxNumPR)))

    for prIdx in range(maxNumPR):
        prTxGainULSFixStr = 'Passive Repeater ' + str(prIdx+1) + ' ULS Fixed Back-to-Back Gain Tx (dBi)'
        prRxGainULSFixStr = 'Passive Repeater ' + str(prIdx+1) + ' ULS Fixed Back-to-Back Gain Rx (dBi)'
        prTxGainStr = 'Passive Repeater ' + str(prIdx+1) + ' Back-to-Back Gain Tx (dBi)'
        prRxGainStr = 'Passive Repeater ' + str(prIdx+1) + ' Back-to-Back Gain Rx (dBi)'
        prRxAntDiameterStr = 'Passive Repeater ' + str(prIdx+1) + ' Rx Ant Diameter (m)'
        prTxAntDiameterStr = 'Passive Repeater ' + str(prIdx+1) + ' Tx Ant Diameter (m)'

        prWidthULSFixStr = 'Passive Repeater ' + str(prIdx+1) + ' ULS Fixed Reflector Width (m)'
        prHeightULSFixStr = 'Passive Repeater ' + str(prIdx+1) + ' ULS Fixed Reflector Height (m)'
        prWidthStr = 'Passive Repeater ' + str(prIdx+1) + ' Reflector Width (m)'
        prHeightStr = 'Passive Repeater ' + str(prIdx+1) + ' Reflector Height (m)'

        fieldnames += [prTxGainULSFixStr,
                       prRxGainULSFixStr,
                       prTxGainStr,
                       prRxGainStr,
                       prRxAntDiameterStr,
                       prTxAntDiameterStr,
                       prWidthULSFixStr,
                       prHeightULSFixStr,
                       prWidthStr,
                       prHeightStr]

    if backwardCompatiblePR:
        fieldnames += ["Passive Receiver Indicator",
                       "Passive Repeater Lat Coords",
                       "Passive Repeater Long Coords",
                       "Passive Repeater Height to Center RAAT (m)"]

    entriesFixed = 0
    with open(outputPath, 'w') as fout:
        csvwriter = csv.writer(fout, delimiter=',')
        csvwriter = csv.DictWriter(fout, fieldnames)
        csvwriter.writeheader()

        for count, row in enumerate(csvreader):
            region = row['Region']
            FRN = row['FRN']
            freq = float(row['Center Frequency (MHz)'])
            bandwidth = float(row['Bandwidth (MHz)'])

            lowFreq = freq - bandwidth/2;
            highFreq = freq + bandwidth/2;

            row['return_FSID'] = ''
            row['Rx Gain (dBi)'] = ''
            row['Tx Gain (dBi)'] = ''

            row['Rx Height to Center RAAT (m)']           = row['Rx Height to Center RAAT ULS (m)']
            row['Rx Diversity Height to Center RAAT (m)'] = row['Rx Diversity Height to Center RAAT ULS (m)']
            row['Tx Height to Center RAAT (m)']           = row['Tx Height to Center RAAT ULS (m)']

            if (highFreq > unii5StartFreqMHz) and (lowFreq < unii5StopFreqMHz):
                uniiband = 5
            elif (highFreq > unii7StartFreqMHz) and (lowFreq < unii7StopFreqMHz):
                uniiband = 7
            elif (highFreq > unii6StartFreqMHz) and (lowFreq < unii6StopFreqMHz):
                uniiband = 6
            elif (highFreq > unii8StartFreqMHz) and (lowFreq < unii8StopFreqMHz):
                uniiband = 8
            else:
                sys.exit('ERROR in fix_params.py: freq found not in UNII-5, UNII-6, UNII-7, UNII-8')

            keyv = tuple([region, FRN, uniiband])
            if keyv in csmap:
                csmap[keyv].append(row)
            else:
                csmap[keyv] = [ row ]
    
        all_cs = list(csmap.keys())
        for keyv in sorted(all_cs):
            matchmap = {}
            for ri in range(len(csmap[keyv])):
                matchmap[ri] = -1
                r = csmap[keyv][ri]
                numPR = int(r['Num Passive Repeater'])
                for prIdx in range(numPR):
                    prNum = prIdx+1
                    rxGainULSStr = 'Passive Repeater ' + str(prNum) + ' ULS Back-to-Back Gain Rx (dBi)'
                    txGainULSStr = 'Passive Repeater ' + str(prNum) + ' ULS Back-to-Back Gain Tx (dBi)'
                    rxGainULSFixStr = 'Passive Repeater ' + str(prNum) + ' ULS Fixed Back-to-Back Gain Rx (dBi)'
                    txGainULSFixStr = 'Passive Repeater ' + str(prNum) + ' ULS Fixed Back-to-Back Gain Tx (dBi)'
                    prAntModelDiameterStr = 'Passive Repeater ' + str(prNum) + ' Ant Model Diameter (m)'
                    rxAntDiameterStr = 'Passive Repeater ' + str(prNum) + ' Rx Ant Diameter (m)'
                    txAntDiameterStr = 'Passive Repeater ' + str(prNum) + ' Tx Ant Diameter (m)'

                    prHeightULSStr    = 'Passive Repeater ' + str(prNum) + ' ULS Reflector Height (m)'
                    prWidthULSStr     = 'Passive Repeater ' + str(prNum) + ' ULS Reflector Width (m)'
                    prHeightULSFixStr = 'Passive Repeater ' + str(prNum) + ' ULS Fixed Reflector Height (m)'
                    prWidthULSFixStr  = 'Passive Repeater ' + str(prNum) + ' ULS Fixed Reflector Width (m)'

                    prHeightAntennaStr = 'Passive Repeater ' + str(prNum) + ' Ant Model Reflector Height (m)'
                    prWidthAntennaStr = 'Passive Repeater ' + str(prNum) + ' Ant Model Reflector Width (m)'

                    prHeightStr = 'Passive Repeater ' + str(prNum) + ' Reflector Height (m)'
                    prWidthStr = 'Passive Repeater ' + str(prNum) + ' Reflector Width (m)'

                    prTypeStr = 'Passive Repeater ' + str(prNum) + ' Ant Type'

                    r[rxGainULSFixStr] = r[rxGainULSStr]
                    r[txGainULSFixStr] = r[txGainULSStr]
                    r[rxAntDiameterStr] = r[prAntModelDiameterStr]
                    r[txAntDiameterStr] = r[prAntModelDiameterStr]
                    r[prHeightULSFixStr] = r[prHeightULSStr]
                    r[prWidthULSFixStr] = r[prWidthULSStr]
                    r[prHeightStr] = ''
                    r[prWidthStr] = ''

                    # R2-AIP-30 (a)
                    if (r[prTypeStr] == 'Ant') or (r[prTypeStr] == 'UNKNOWN'):
                        rxFlag = False
                        txFlag = False
                        rxGainULS = 0.0
                        txGainULS = 0.0
                        if (r[rxGainULSStr].strip() != ''):
                            rxGainULS = float(r[rxGainULSStr])
                            if ((rxGainULS >= 32.0) and (rxGainULS <= 48.0)):
                                rxFlag = True
                        if (r[txGainULSStr].strip() != ''):
                            txGainULS = float(r[txGainULSStr])
                            if ((txGainULS >= 32.0) and (txGainULS <= 48.0)):
                                txFlag = True
                        if (r[prTypeStr] == 'UNKNOWN'):
                            if rxFlag or txFlag:
                                r[prTypeStr] = 'Ant'
                            else:
                                r[prTypeStr] = 'Ref'

                        if (r[prTypeStr] == 'Ant'):
                            # R2-AIP-30 (d)
                            if rxFlag and (not txFlag):
                                r[txGainULSFixStr] = r[rxGainStrULS]
                            # R2-AIP-30 (e)
                            elif txFlag and (not rxFlag):
                                r[rxGainULSFixStr] = r[txGainULSStr]

                    if (r[prTypeStr] == 'Ref'):
                        # R2-AIP-30 (b)
                        if (r[prHeightULSStr].strip() == '') or (r[prWidthULSStr].strip() == ''):
                            r[prHeightULSFixStr] = str(4.88)
                            r[prWidthULSFixStr] = str(6.10)
                        # 2022.11.08: Need to confirm this
                        elif (float(r[prHeightULSStr])+0.1 < 1.83) or (float(r[prWidthULSStr])+0.1 < 2.44):
                            r[prHeightULSFixStr] = str(4.88)
                            r[prWidthULSFixStr] = str(6.10)

                        prHeightULS = float(r[prHeightULSFixStr])
                        prWidthULS = float(r[prWidthULSFixStr])
                        # R2-AIP-29-b-iii-1
                        if (r[prHeightAntennaStr].strip() == '') or (r[prWidthAntennaStr].strip() == ''):
                            prHeight = prHeightULS
                            prWidth = prWidthULS
                        else:
                            prHeightAnt = float(r[prHeightAntennaStr])
                            prWidthAnt = float(r[prWidthAntennaStr])

                            # R2-AIP-29-b-iii-2
                            if (abs(prHeightAnt-prHeightULS) <= 0.1) and (abs(prWidthAnt-prWidthULS) <= 0.1):
                                prHeight = prHeightULS
                                prWidth = prWidthULS

                            else:
                                ulsTypicalFlag = isTypicalReflectorDimension(prHeightULS, prWidthULS)
                                antTypicalFlag = isTypicalReflectorDimension(prHeightAnt, prWidthAnt)

                                # R2-AIP-29-b-iii-3
                                # R2-AIP-29-b-iii-5
                                if (ulsTypicalFlag == antTypicalFlag):
                                    if (prHeightULS*prWidthULS > prHeightAnt*prWidthAnt):
                                        prHeight = prHeightULS
                                        prWidth = prWidthULS
                                    else:
                                        prHeight = prHeightAnt
                                        prWidth = prWidthAnt

                                # R2-AIP-29-b-iii-4
                                elif ulsTypicalFlag:
                                    prHeight = prHeightULS
                                    prWidth = prWidthULS
                                elif antTypicalFlag:
                                    prHeight = prHeightAnt
                                    prWidth = prWidthAnt
                                # R2-AIP-29-b-iv
                                else:
                                    prHeight = prHeightAnt
                                    prWidth = prWidthAnt

                        r[prHeightStr] = str(prHeight)
                        r[prWidthStr] = str(prWidth)

                if backwardCompatiblePR:
                    if numPR == 0:
                        r["Passive Receiver Indicator"] = "N"
                        r["Passive Repeater Lat Coords"] = ""
                        r["Passive Repeater Long Coords"] = ""
                        r["Passive Repeater Height to Center RAAT (m)"] = ""
                    else:
                        r["Passive Receiver Indicator"] = "Y"
                        r["Passive Repeater Lat Coords"] = r['Passive Repeater ' + str(numPR) + ' Lat Coords']
                        r["Passive Repeater Long Coords"] = r['Passive Repeater ' + str(numPR) + ' Long Coords']
                        r["Passive Repeater Height to Center RAAT (m)"] = r['Passive Repeater ' + str(numPR) + ' Height to Center RAAT (m)']


            for ri in range(len(csmap[keyv])):
                if keyv[0] == 'US':
                    r = csmap[keyv][ri]
                    if (keyv[2] == 5):
                        Fc_unii = (unii5StartFreqMHz + unii5StopFreqMHz)*0.5e6
                    elif (keyv[2] == 7):
                        Fc_unii = (unii7StartFreqMHz + unii7StopFreqMHz)*0.5e6
                    elif (keyv[2] == 8):
                        Fc_unii = (unii8StartFreqMHz + unii8StopFreqMHz)*0.5e6
                    else:
                        sys.exit('ERROR in fix_params.py: freq found not in UNII-5, UNII-7, UNII-8')
    
                    numPR = int(r['Num Passive Repeater'])
    
                    for prNum in range(numPR+1):
    
                        if prNum == 0:
                            numDirection = 1
                            antType = 'Ant'
                        else:
                            numDirection = 2
                            antType = r['Passive Repeater ' + str(prNum) + ' Ant Type']
    
                        for direction in range(numDirection):
                            if direction == 0:
                               fwd = 'Rx'
                               ret = 'Tx'
                            else:
                               fwd = 'Tx'
                               ret = 'Rx'
    
                            fwdGain = ''
                            if prNum == 0:
                                fwdGainStrULS = 'Rx Gain ULS (dBi)'
                                fwdGainStr    = 'Rx Gain (dBi)'
                                retGainStrULS = 'Tx Gain ULS (dBi)'
                                retGainStr    = 'Tx Gain (dBi)'
                                fwdAntDiameterStr = 'Rx Ant Diameter (m)'
                                retAntDiameterStr = 'Tx Ant Diameter (m)'
                                fwdHeightStr = 'Rx Height to Center RAAT (m)'
                                retHeightStr = 'Tx Height to Center RAAT (m)'
                                fwdAntMidbandGain = 'Rx Ant Midband Gain (dB)'
                                retAntMidbandGain = 'Tx Ant Midband Gain (dB)'
                            else:
                                fwdGainStrULS = 'Passive Repeater ' + str(prNum) + ' ULS Fixed Back-to-Back Gain ' + fwd + ' (dBi)'
                                fwdGainStr    = 'Passive Repeater ' + str(prNum) + ' Back-to-Back Gain ' + fwd + ' (dBi)'
                                retGainStrULS = 'Passive Repeater ' + str(numPR+1-prNum) + ' ULS Fixed Back-to-Back Gain ' + ret + ' (dBi)'
                                retGainStr    = 'Passive Repeater ' + str(numPR+1-prNum) + ' Back-to-Back Gain ' + ret + ' (dBi)'
                                fwdAntDiameterStr = 'Passive Repeater ' + str(prNum) + ' ' + fwd + ' Ant Diameter (m)'
                                retAntDiameterStr = 'Passive Repeater ' + str(numPR+1-prNum) + ' ' + ret + ' Ant Diameter (m)'
                                fwdHeightStr = 'Passive Repeater ' + str(prNum) + ' Height to Center RAAT (m)'
                                retHeightStr = 'Passive Repeater ' + str(numPR+1-prNum) + ' Height to Center RAAT (m)'
                                fwdAntMidbandGain = 'Passive Repeater ' + str(prNum) + ' Ant Model Midband Gain (dB)'
                                retAntMidbandGain = 'Passive Repeater ' + str(numPR+1-prNum) + ' Ant Model Midband Gain (dB)'
    
                            # R2-AIP-05 (e)
                            if (antType == 'Ant') and (r[fwdGainStrULS].strip() == ''):
                                if (keyv[2] == 5):
                                    fwdGain = 38.8
                                elif (keyv[2] == 7):
                                    fwdGain = 39.5
                                elif (keyv[2] == 8):
                                    fwdGain = 39.5
                                else:
                                    sys.exit('ERROR in fix_params.py: freq found not in UNII-5, UNII-7, UNII-8')
    
                                # 6 ft converted to m
                                r[fwdAntDiameterStr] = 6*12*2.54/100
    
                            # R2-AIP-05 (a)
                            elif (antType == 'Ant') and (float(r[fwdAntDiameterStr]) != -1):
                                fwdGainULS = float(r[fwdGainStrULS])
                                Drx = float(r[fwdAntDiameterStr])
                                Eta = 0.55
                                Gtypical = 10*math.log10(Eta*(math.pi*Fc_unii*Drx/c)**2)
                                if (fwdGainULS >= Gtypical - 0.7) and (fwdGainULS <= Gtypical + 0.7):
                                    fwdGain = fwdGainULS
                                elif (r[fwdAntMidbandGain].strip() != ''):
                                    fwdGain = float(r[fwdAntMidbandGain])
                                else:
                                    fwdGain = Gtypical
    
                            # R2-AIP-05 (b): Does not apply to passive repeaters
                            elif (prNum == 0) and (float(r[retAntDiameterStr]) != -1) and (r[retGainStrULS].strip() != ''):
                                fwdGainULS = float(r[fwdGainStrULS])
                                retGainULS = float(r[retGainStrULS])
                                if (abs(fwdGainULS - retGainULS) <= 0.05):
                                    D = float(r[retAntDiameterStr])
                                    Eta = 0.55
                                    Gtypical = 10*math.log10(Eta*(math.pi*Fc_unii*D/c)**2)
                                    if (fwdGainULS >= Gtypical - 0.7) and (fwdGainULS <= Gtypical + 0.7):
                                        fwdGain = fwdGainULS
                                    elif (r[retAntMidbandGain].strip() != ''):
                                        fwdGain = float(r[retAntMidbandGain])
                                    else:
                                        fwdGain = Gtypical
                                    r[fwdAntDiameterStr] = D
    
                            if (antType == 'Ant') and (fwdGain == ''):
                                matchFlagFwdGain = True
                            else:
                                matchFlagFwdGain = False
    
                            if float(r[fwdHeightStr]) == -1:
                                matchFlagFwdHeight = True
                            else:
                                matchFlagFwdHeight = False
    
                            if float(r[retHeightStr]) == -1:
                                matchFlagRetHeight = True
                            else:
                                matchFlagRetHeight = False
    
                            if (matchFlagFwdGain or matchFlagFwdHeight or matchFlagRetHeight) and r['return_FSID'].strip() == '':
                                rxLat = float(r['Rx Lat Coords'])
                                rxLon = float(r['Rx Long Coords'])
                                txLat = float(r['Tx Lat Coords'])
                                txLon = float(r['Tx Long Coords'])
                                foundMatch = False
                                foundCandidate = False
                                for (mi, m) in enumerate(csmap[keyv]):
                                    if (not foundMatch) and (mi != ri):
                                        m_rxLat = float(m['Rx Lat Coords'])
                                        m_rxLon = float(m['Rx Long Coords'])
                                        m_txLat = float(m['Tx Lat Coords'])
                                        m_txLon = float(m['Tx Long Coords'])
                                        m_numPR = int(m['Num Passive Repeater'])
                                        if (     (abs(rxLon-m_txLon) < 2.78e-4) and (abs(rxLat-m_txLat) < 2.78e-4)
                                             and (abs(txLon-m_rxLon) < 2.78e-4) and (abs(txLat-m_rxLat) < 2.78e-4)
                                             and (numPR == m_numPR) ):
                                            prIdx = 1
                                            prMatch = True
                                            while (prMatch and (prIdx <= numPR)):
                                                prLat = float(r['Passive Repeater ' + str(prIdx) + ' Lat Coords'])
                                                prLon = float(r['Passive Repeater ' + str(prIdx) + ' Long Coords'])
                                                prType = r['Passive Repeater ' + str(prIdx) + ' Ant Type']
                                                m_prLat = float(m['Passive Repeater ' + str(numPR+1-prIdx) + ' Lat Coords'])
                                                m_prLon = float(m['Passive Repeater ' + str(numPR+1-prIdx) + ' Long Coords'])
                                                m_prType = r['Passive Repeater ' + str(numPR+1-prIdx) + ' Ant Type']
                                                if (abs(prLon-m_prLon) < 2.78e-4) and (abs(prLat-m_prLat) < 2.78e-4) and (prType == m_prType):
                                                    prIdx += 1
                                                else:
                                                    prMatch = False
                                            if prMatch:
                                                if m['return_FSID'].strip() == '':
                                                    foundMatch = True
                                                    matchi = mi
                                                else:
                                                    foundCandidate = True
                                                    candidatei = mi
                                if foundMatch:
                                    m = csmap[keyv][matchi]
                                    csmap[keyv][ri]['return_FSID'] = m['FSID']
                                    csmap[keyv][matchi]['return_FSID'] = r['FSID']
                                    matchmap[ri] = matchi
                                    matchmap[matchi] = ri
                                    logFile.write('Matched FSID: ' + str(r['FSID']) + ' and ' + str(m['FSID']) + ' numPR = ' + str(numPR) + '\n')
                                elif foundCandidate:
                                    m = csmap[keyv][candidatei]
                                    csmap[keyv][ri]['return_FSID'] = '-' + m['FSID']
                                    matchmap[ri] = candidatei
                                    logFile.write('FSID: ' + str(r['FSID']) + ' using candidate match ' + str(m['FSID']) + ' numPR = ' + str(numPR) + '\n')
    
                            matchi = matchmap[ri]
    
                            if matchFlagFwdHeight:
                                r[fwdHeightStr] = '42.5'
                                if matchi != -1:
                                    m = csmap[keyv][matchi]
    
                                    if m[retHeightStr].strip() != '':
                                        m_retHeightAGL = float(m[retHeightStr])
                                        if m_retHeightAGL > 1.5:
                                            r[fwdHeightStr] = str(m_retHeightAGL)
    
                            if matchFlagRetHeight:
                                r[retHeightStr] = '42.5'
                                if matchi != -1:
                                    m = csmap[keyv][matchi]
    
                                    if m[fwdHeightStr].strip() != '':
                                        m_fwdHeightAGL = float(m[fwdHeightStr])
                                        if m_fwdHeightAGL > 1.5:
                                            r[retHeightStr] = str(m_fwdHeightAGL)
    
                            # R2-AIP-05 (c)
                            if matchFlagFwdGain:
                                if matchi != -1:
                                    m = csmap[keyv][matchi]
                                    if (float(m[retAntDiameterStr]) != -1) and (m[retGainStrULS].strip() != ''):
    
                                        fwdGainULS = float(r[fwdGainStrULS])
                                        m_txGainULS = float(m[retGainStrULS])
                                        if (abs(fwdGainULS - m_txGainULS) <= 0.05):
                                            D = float(m[retAntDiameterStr])
                                            Eta = 0.55
                                            Gtypical = 10*math.log10(Eta*(math.pi*Fc_unii*D/c)**2)
                                            if (fwdGainULS >= Gtypical - 0.7) and (fwdGainULS <= Gtypical + 0.7):
                                                fwdGain = fwdGainULS
                                            elif (m[retAntMidbandGain].strip() != ''):
                                                fwdGain = float(m[retAntMidbandGain])
                                            else:
                                                fwdGain = Gtypical
                                            r[fwdAntDiameterStr] = str(D)
    
                            # R2-AIP-05 (d)
                            if (antType == 'Ant') and (fwdGain == ''):
                                fwdGainULS = float(r[fwdGainStrULS])
                                if fwdGainULS < 32.0:
                                    fwdGain = 32.0
                                elif fwdGainULS > 48.0:
                                    fwdGain = 48.0
                                else:
                                    fwdGain = fwdGainULS
                                oneOverSqrtEta = 1.0/math.sqrt(0.55)
                                D = (c/(math.pi*Fc_unii))*(10**((fwdGain)/20))*oneOverSqrtEta
                                csmap[keyv][ri][fwdAntDiameterStr] = str(D)
    
                            if (antType == 'Ant'):
                                r[fwdGainStr] = str(fwdGain)
    
                    ####################################################################################
                    # R2-AIP-08 Diversity Antenna Gain and Diameter                                    #
                    # R2-AIP-15 Diversity Antenna Height                                               #
                    ####################################################################################
                    rxDiameterStr = 'Rx Ant Diameter (m)'
                    rxGainStrULS  = 'Rx Gain ULS (dBi)'
                    rxGainStr     = 'Rx Gain (dBi)'
                    diversityDiameterStr = 'Rx Diversity Ant Diameter (m)'
                    diversityGainStrULS  = 'Rx Diversity Gain ULS (dBi)'
                    diversityGainStr     = 'Rx Diversity Gain (dBi)'
                    rxAntModelMatchedStr = 'Rx Ant Model Name Matched'
                    rxHeightStr          = 'Rx Height to Center RAAT (m)'
                    diversityHeightStr   = 'Rx Diversity Height to Center RAAT (m)'
    
                    if (r[diversityGainStrULS] != '') and (float(r[diversityGainStrULS]) != 0.0):
                        if (r[rxGainStrULS] != '') and (r[rxAntModelMatchedStr] != '') and (abs(float(r[rxGainStrULS])-float(r[diversityGainStrULS])) < 0.05):
                            r[diversityGainStr] = r[rxGainStr]
                            r[diversityDiameterStr] = r[rxDiameterStr]
                        else:
                            diversityGainULS  = float(r[diversityGainStrULS])
                            if (diversityGainULS >= 28.0) and (diversityGainULS <= 48.0):
                                r[diversityGainStr] = r[diversityGainStrULS]
                            else:
                                r[diversityGainStr] = r[rxGainStrULS]
    
                            diversityGain  = float(r[diversityGainStr])
                            oneOverSqrtEta = 1.0/math.sqrt(0.55)
                            D = (c/(math.pi*Fc_unii))*(10**((diversityGain)/20))*oneOverSqrtEta
                            r[diversityDiameterStr] = str(D)
    
                        if (r[diversityHeightStr] == ''):
                            rxHeight = float(r[rxHeightStr])
                            if (rxHeight < 14.0):
                                r[diversityHeightStr] = str(rxHeight + 11)
                            else:
                                r[diversityHeightStr] = str(rxHeight - 11)
    
                        if (float(r[diversityHeightStr]) < 1.5):
                            r[diversityHeightStr] = str(1.5)
                    ####################################################################################
    
                    ####################################################################################
                    # R2-AIP-17 Near Field Adjustment                                                  #
                    ####################################################################################
                    nearFieldDiameterStr   = 'Rx Near Field Ant Diameter (m)'
                    nearFieldDistLimitStr  = 'Rx Near Field Dist Limit (m)'
                    nearFieldEfficiencyStr = 'Rx Near Field Ant Efficiency'
    
                    if (r[rxAntModelMatchedStr] != ''):
                        rxNearFieldAntennaDiameter = float(r[rxDiameterStr])
                        method = 1
                        if (keyv[2] == 5):
                            if (abs(rxNearFieldAntennaDiameter - 3.0*12*2.54*0.01) <= 0.01):
                                gainRangeMin = 32.0
                                gainRangeMax = 34.5
                                foundDiameter = True
                            elif (abs(rxNearFieldAntennaDiameter - 4.0*12*2.54*0.01) <= 0.01):
                                gainRangeMin = 34.5
                                gainRangeMax = 37.55
                                foundDiameter = True
                            elif (abs(rxNearFieldAntennaDiameter - 6.0*12*2.54*0.01) <= 0.01):
                                gainRangeMin = 37.55
                                gainRangeMax = 40.35
                                foundDiameter = True
                            elif (abs(rxNearFieldAntennaDiameter - 8.0*12*2.54*0.01) <= 0.01):
                                gainRangeMin = 40.35
                                gainRangeMax = 42.55
                                foundDiameter = True
                            elif (abs(rxNearFieldAntennaDiameter - 10.0*12*2.54*0.01) <= 0.01):
                                gainRangeMin = 42.55
                                gainRangeMax = 44.55
                                foundDiameter = True
                            elif (abs(rxNearFieldAntennaDiameter - 12.0*12*2.54*0.01) <= 0.01):
                                gainRangeMin = 44.55
                                gainRangeMax = 46.15
                                foundDiameter = True
                            elif (abs(rxNearFieldAntennaDiameter - 15.0*12*2.54*0.01) <= 0.01):
                                gainRangeMin = 46.15
                                gainRangeMax = 48.0
                                foundDiameter = True
                            else:
                                foundDiameter = False
                        else:
                            if (abs(rxNearFieldAntennaDiameter - 3.0*12*2.54*0.01) <= 0.01):
                                gainRangeMin = 32.0
                                gainRangeMax = 34.55
                                foundDiameter = True
                            elif (abs(rxNearFieldAntennaDiameter - 4.0*12*2.54*0.01) <= 0.01):
                                gainRangeMin = 34.55
                                gainRangeMax = 37.65
                                foundDiameter = True
                            elif (abs(rxNearFieldAntennaDiameter - 6.0*12*2.54*0.01) <= 0.01):
                                gainRangeMin = 37.65
                                gainRangeMax = 40.55
                                foundDiameter = True
                            elif (abs(rxNearFieldAntennaDiameter - 8.0*12*2.54*0.01) <= 0.01):
                                gainRangeMin = 40.55
                                gainRangeMax = 42.75
                                foundDiameter = True
                            elif (abs(rxNearFieldAntennaDiameter - 10.0*12*2.54*0.01) <= 0.01):
                                gainRangeMin = 42.75
                                gainRangeMax = 44.55
                                foundDiameter = True
                            elif (abs(rxNearFieldAntennaDiameter - 12.0*12*2.54*0.01) <= 0.01):
                                gainRangeMin = 44.55
                                gainRangeMax = 46.25
                                foundDiameter = True
                            elif (abs(rxNearFieldAntennaDiameter - 15.0*12*2.54*0.01) <= 0.01):
                                gainRangeMin = 46.25
                                gainRangeMax = 48.0
                                foundDiameter = True
                            else:
                                foundDiameter = False
    
                    elif (r[rxGainStrULS] == ''):
                        diameterFt = 6.0
                        rxNearFieldAntennaDiameter = diameterFt*12.0*2.54*0.01 # convert ft to m
                        method = 3
                    elif (keyv[2] == 5):
                        rxGainULS = float(r[rxGainStrULS])
                        if ( (rxGainULS >= 32.0) and (rxGainULS <= 48.0) ):
                            #****************************************************************************#
                            #* Table 3: U-NII-5 Antenna Size versus Gain                                *#
                            #****************************************************************************#
                            if (rxGainULS <= 34.35):
                                diameterFt = 3.0
                            elif (rxGainULS <= 37.55):
                                diameterFt = 4.0
                            elif (rxGainULS <= 40.35):
                                diameterFt = 6.0
                            elif (rxGainULS <= 42.55):
                                diameterFt = 8.0
                            elif (rxGainULS <= 44.55):
                                diameterFt = 10.0
                            elif (rxGainULS <= 46.15):
                                diameterFt = 12.0
                            else:
                                diameterFt = 15.0
                            #****************************************************************************#
                            rxNearFieldAntennaDiameter = diameterFt*12.0*2.54*0.01 # convert ft to m
                            method = 2
                        else:
                            diameterFt = 6.0
                            rxNearFieldAntennaDiameter = diameterFt*12.0*2.54*0.01 # convert ft to m
                            method = 3
                    else:
                        rxGainULS = float(r[rxGainStrULS])
                        if ( (rxGainULS >= 32.0) and (rxGainULS <= 48.0) ):
                            #****************************************************************************#
                            #* Table 4: U-NII-7 Antenna Size versus Gain                                *#
                            #****************************************************************************#
                            if (rxGainULS <= 34.55):
                                diameterFt = 3.0
                            elif (rxGainULS <= 37.65):
                                diameterFt = 4.0
                            elif (rxGainULS <= 40.55):
                                diameterFt = 6.0
                            elif (rxGainULS <= 42.75):
                                diameterFt = 8.0
                            elif (rxGainULS <= 44.55):
                                diameterFt = 10.0
                            elif (rxGainULS <= 46.25):
                                diameterFt = 12.0
                            else:
                                diameterFt = 15.0
                            #****************************************************************************#
                            rxNearFieldAntennaDiameter = diameterFt*12.0*2.54*0.01     # convert ft to m
                            method = 2
                        else:
                            diameterFt = 6.0
                            rxNearFieldAntennaDiameter = diameterFt*12.0*2.54*0.01 # convert ft to m
                            method = 3
    
                    if (r[rxGainStrULS] == ''):
                        effDB = -2.6
                    else:
                        rxGainULS = float(r[rxGainStrULS])
                        if (method == 1) and (not foundDiameter):
                            effDB = -2.6
                        elif (method == 1) and ((rxGainULS < gainRangeMin-0.3) or (rxGainULS > gainRangeMax+0.3)):
                            effDB = -2.6
                        elif method == 3:
                            effDB = -2.6
                        else:
                            effDB = rxGainULS + 20.0*math.log10(c/(math.pi*Fc_unii*rxNearFieldAntennaDiameter))
    
                    rxNearFieldDistLimit = 2*Fc_unii*rxNearFieldAntennaDiameter*rxNearFieldAntennaDiameter/c
    
                    rxNearFieldAntEfficiency = 10**(effDB/10.0)
                    if (rxNearFieldAntEfficiency < 0.4):
                        rxNearFieldAntEfficiency = 0.4
                    elif (rxNearFieldAntEfficiency > 0.7):
                        rxNearFieldAntEfficiency = 0.7
    
                    r[nearFieldDiameterStr]   = str(rxNearFieldAntennaDiameter)
                    r[nearFieldDistLimitStr]  = str(rxNearFieldDistLimit)
                    r[nearFieldEfficiencyStr] = str(rxNearFieldAntEfficiency)
                    ####################################################################################

            for r in csmap[keyv]:
                numPR = int(r['Num Passive Repeater'])
                for prIdx in range(numPR):
                    prNum = prIdx+1
                    prType = r['Passive Repeater ' + str(prNum) + ' Ant Type']
                    if (prType == 'Ant') and False:
                        rxGainStr = 'Passive Repeater ' + str(prNum) + ' Back-to-Back Gain Rx (dBi)'
                        txGainStr = 'Passive Repeater ' + str(prNum) + ' Back-to-Back Gain Tx (dBi)'
                        rxGain = r[rxGainStr]
                        txGain = r[txGainStr]
                        txFlag = False
                        if txGain != '':
                            if ((float(txGain) >= 32.0) and (float(txGain) <= 48.0)):
                                txFlag = True
                        if not txFlag:
                            r[txGainStr] = rxGain
                csvwriter.writerow(r)
            file_handle.close()
