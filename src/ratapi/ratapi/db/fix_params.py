import csv
import sys
import math
from os.path import exists

csmap = {}
c  = 2.99792458e8

typicalReflectorDimensions = [
    (1.8,2.4),
    (2.4,3),
    (2.4,3.7),
    (3,4.6),
    (3,4.9),
    (3,7.3),
    (3.7,4.9),
    (4.3,4.9),
    (4.9,6.1),
    (4.9,7.3),
    (6.1,7.3),
    (6.1,9.1),
    (6.1,9.8),
    (7.3,9.1),
    (9.1,9.8),
    (9.1,12.2),
    (9.1,14.6),
    (12.2,15.2) ]

def isTypicalReflectorDimension(height, width):
    found = False
    for refDim in typicalReflectorDimensions:
        typicalHeight = refDim[0]
        typicalWidth = refDim[1]
        if (abs(typicalHeight - height) <= 0.01) and (abs(typicalWidth - width) <= 0.01):
            found = False
            break
    return found

def fixParams(inputPath, outputPath, logFile):
    logFile.write('Fixing parameters' + '\n')

    file_handle = open(inputPath, 'rb')
    csvreader = csv.DictReader(file_handle)

    fieldnames = csvreader.fieldnames + ['return_FSID', 'Rx Gain (dBi)', 'Tx Gain (dBi)']

    maxNumPR = 0
    for field in csvreader.fieldnames:
        wordList = field.split()
        if (wordList[0] == 'Passive') and (wordList[1] == 'Repeater'):
            n = int(wordList[2])
            if (n > maxNumPR):
                maxNumPR = n
    print 'maxNumPR = ' + str(maxNumPR)

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

    entriesFixed = 0
    with open(outputPath, 'w') as fout:
        csvwriter = csv.writer(fout, delimiter=',')
        csvwriter = csv.DictWriter(fout, fieldnames)
        csvwriter.writeheader()

        for count, row in enumerate(csvreader):
            FRN = row['FRN']
            freq = float(row['Center Frequency (MHz)'])

            row['return_FSID'] = ''
            row['Rx Gain (dBi)'] = ''
            row['Tx Gain (dBi)'] = ''

            if (freq >= 5925.0) and (freq <= 6425.0):
                uniiband = 5
            elif (freq >= 6525.0) and (freq <= 6875.0):
                uniiband = 7
            else:
                sys.exit('ERROR: freq found not in UNII-5 or UNII-7')

            keyv = tuple([FRN, uniiband])
            if keyv in csmap:
                csmap[keyv].append(row)
            else:
                csmap[keyv] = [ row ]
    
        all_cs = csmap.keys()
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
                        if (r[prHeightULSStr].strip() == '') or (r[prWidthULSStr].strip() == ''):
                            r[prHeightULSFixStr] = str(16.0*12*2.54/100)
                            r[prWidthULSFixStr] = str(20.0*12*2.54/100)

                        prHeightULS = float(r[prHeightULSFixStr])
                        prWidthULS = float(r[prWidthULSFixStr])
                        # R2-AIP-29-b-iii-1 and R2-AIP-29-b-iii-2 
                        if (prHeightAntennaStr.strip() == '') or (prWidthAntennaStr.strip() == ''):
                            prHeight = prHeightULS
                            prWidth = prWidthULS
                        else:
                            prHeightAnt = float(r[prHeightAntennaStr])
                            prWidthAnt = float(r[prWidthAntennaStr])

                            # R2-AIP-29-b-iii-3
                            if (abs(prHeightAnt-prHeightULS) <= 0.01) and (abs(prWidthAnt-prWidthULS) <= 0.01):
                                prHeight = prHeightULS
                                prWidth = prWidthULS

                            else:
                                ulsTypicalFlag = isTypicalReflectorDimension(prHeightULS, prWidthULS)
                                antTypicalFlag = isTypicalReflectorDimension(prHeightAnt, prWidthAnt)

                                # R2-AIP-29-b-iii-4
                                # R2-AIP-29-b-iii-6
                                if (ulsTypicalFlag == antTypicalFlag):
                                    if (prHeightULS*prWidthULS > prHeightAnt*prWidthAnt):
                                        prHeight = prHeightULS
                                        prWidth = prWidthULS
                                    else:
                                        prHeight = prHeightAnt
                                        prWidth = prWidthAnt

                                # R2-AIP-29-b-iii-5
                                elif ulsTypicalFlag:
                                    prHeight = prHeightULS
                                    prWidth = prWidthULS
                                elif antTypicalFlag:
                                    prHeight = prHeightAnt
                                    prWidth = prWidthAnt

                        r[prHeightStr] = str(prHeight)
                        r[prWidthStr] = str(prWidth)

            for ri in range(len(csmap[keyv])):
                r = csmap[keyv][ri]
                if (keyv[1] == 5):
                    Fc_unii = 6.175e9
                elif (keyv[1] == 7):
                    Fc_unii = 6.7e9
                else:
                    sys.exit('ERROR: freq found not in UNII-5 or UNII-7')

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
                            if (keyv[1] == 5):
                                fwdGain = 38.8
                            elif (keyv[1] == 7):
                                fwdGain = 39.5
                            else:
                                sys.exit('ERROR: freq found not in UNII-5 or UNII-7')

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
                            if (abs(fwdGainULS - retGainULS) <= 0.1):
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
                                                foundMatch = True;
                                                matchi = mi
                                            else:
                                                foundCandidate = True;
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
                                    if (abs(fwdGainULS - m_txGainULS) <= 0.1):
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
