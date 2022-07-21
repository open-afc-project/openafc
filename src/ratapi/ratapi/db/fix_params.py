import csv
import sys
import math
from os.path import exists

csmap = {}
c  = 2.99792458e8

def fixParams(inputPath, outputPath, logFile):
    logFile.write('Fixing parameters' + '\n')

    entriesFixed = 0
    with open(inputPath, 'r') as f:
        with open(outputPath, 'w') as fout:
            csvreader = csv.reader(f, delimiter=',')
            csvwriter = csv.writer(fout, delimiter=',')
            firstRow = True
            for row in csvreader:
                if firstRow:
                    row.append("return_FSID")
                    row.append("Rx Gain (dBi)")
                    row.append("Tx Gain (dBi)")
                    firstRow = False
                    FSIDIdx = -1
                    FRNIdx = -1
                    freqIdx = -1
                    txLatIdx = -1
                    txLonIdx = -1
                    txHeightAGLIdx = -1
                    rxLatIdx = -1
                    rxLonIdx = -1
                    rxHeightAGLIdx = -1
                    txHeightAGLIdx = -1
                    return_FSIDIdx = -1
                    rxAntDiameterIdx = -1
                    txAntDiameterIdx = -1
                    rxMidbandGainIdx = -1
                    txMidbandGainIdx = -1
                    rxGainULSIdx = -1
                    txGainULSIdx = -1
                    rxGainIdx = -1
                    txGainIdx = -1

                    for fieldIdx, field in enumerate(row):
                        if field == "FSID":
                            FSIDIdx = fieldIdx
                        elif field == "FRN":
                            FRNIdx = fieldIdx
                        elif field == "Center Frequency (MHz)":
                            freqIdx = fieldIdx
                        elif field == "Tx Lat Coords":
                            txLatIdx = fieldIdx
                        elif field == "Tx Long Coords":
                            txLonIdx = fieldIdx
                        elif field == "Tx Height to Center RAAT (m)":
                            txHeightAGLIdx = fieldIdx
                        elif field == "Rx Lat Coords":
                            rxLatIdx = fieldIdx
                        elif field == "Rx Long Coords":
                            rxLonIdx = fieldIdx
                        elif field == "Rx Height to Center RAAT (m)":
                            rxHeightAGLIdx = fieldIdx
                        elif field == "Tx Height to Center RAAT (m)":
                            txHeightAGLIdx = fieldIdx
                        elif field == "return_FSID":
                            return_FSIDIdx = fieldIdx
                        elif field == "Rx Ant Diameter (m)":
                            rxAntDiameterIdx = fieldIdx
                        elif field == "Tx Ant Diameter (m)":
                            txAntDiameterIdx = fieldIdx
                        elif field == "Rx Ant Midband Gain (dB)":
                            rxMidbandGainIdx = fieldIdx
                        elif field == "Tx Ant Midband Gain (dB)":
                            txMidbandGainIdx = fieldIdx
                        elif field == "Rx Gain ULS (dBi)":
                            rxGainULSIdx = fieldIdx
                        elif field == "Tx Gain ULS (dBi)":
                            txGainULSIdx = fieldIdx
                        elif field == "Rx Gain (dBi)":
                            rxGainIdx = fieldIdx
                        elif field == "Tx Gain (dBi)":
                            txGainIdx = fieldIdx

                    if FSIDIdx == -1:
                        sys.exit('ERROR: FSID not found')
                    if FRNIdx == -1:
                        sys.exit('ERROR:  "FRN not found')
                    if freqIdx == -1:
                        sys.exit('ERROR:  "Center Frequency (MHz) not found')
                    if txLatIdx == -1:
                        sys.exit('ERROR:  "Tx Lat Coords not found')
                    if txLonIdx == -1:
                        sys.exit('ERROR:  "Tx Long Coords not found')
                    if txHeightAGLIdx == -1:
                        sys.exit('ERROR:  "Tx Height to Center RAAT (m) not found')
                    if rxLatIdx == -1:
                        sys.exit('ERROR:  "Rx Lat Coords not found')
                    if rxLonIdx == -1:
                        sys.exit('ERROR:  "Rx Long Coords not found')
                    if rxHeightAGLIdx == -1:
                        sys.exit('ERROR:  "Rx Height to Center RAAT (m) not found')
                    if txHeightAGLIdx == -1:
                        sys.exit('ERROR:  "Tx Height to Center RAAT (m) not found')
                    if return_FSIDIdx == -1:
                        sys.exit('ERROR:  "return_FSID not found')
                    if rxAntDiameterIdx == -1:
                        sys.exit('ERROR:  "Rx Ant Diameter (m) not found')
                    if txAntDiameterIdx == -1:
                        sys.exit('ERROR:  "Tx Ant Diameter (m) not found')
                    if rxMidbandGainIdx == -1:
                        sys.exit('ERROR:  "Rx Ant Midband Gain (dB) not found')
                    if txMidbandGainIdx == -1:
                        sys.exit('ERROR:  "Tx Ant Midband Gain (dB) not found')
                    if rxGainULSIdx == -1:
                        sys.exit('ERROR:  "Rx Gain ULS (dBi) not found')
                    if txGainULSIdx == -1:
                        sys.exit('ERROR:  "Tx Gain ULS (dBi) not found')
                    if rxGainIdx == -1:
                        sys.exit('ERROR:  "Rx Gain (dBi) not found')
                    if txGainIdx == -1:
                        sys.exit('ERROR:  "Tx Gain (dBi) not found')

                    csvwriter.writerow(row)
                else:
                    row.append("") # return_FSID
                    row.append("") # Rx Gain (dBi)
                    row.append("") # Tx Gain (dBi)

                    FSID = row[FSIDIdx]
                    FRN = row[FRNIdx]
                    freq = float(row[freqIdx])
                    txLat = float(row[txLatIdx])
                    txLon = float(row[txLonIdx])
                    txHeightAGL = row[txHeightAGLIdx]
                    rxLat = float(row[rxLatIdx])
                    rxLon = float(row[rxLonIdx])
                    rxHeightAGL = row[rxHeightAGLIdx]
                    return_FSID = row[return_FSIDIdx]

                    if (freq >= 5925.0) and (freq <= 6425.0):
                        uniiband = 5
                    elif (freq >= 6525.0) and (freq <= 6875.0):
                        uniiband = 7
                    else:
                        sys.exit('ERROR:  "freq found not in UNII-5 or UNII-7')

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
                for ri in range(len(csmap[keyv])):
                    r = csmap[keyv][ri]
                    if (keyv[1] == 5):
                        Fc_unii = 6.175e9
                    elif (keyv[1] == 7):
                        Fc_unii = 6.7e9
                    else:
                        sys.exit('ERROR:  "freq found not in UNII-5 or UNII-7')

                    rxGain = ""

                    # R2-AIP-05 (e)
                    if (r[rxGainULSIdx].strip() == ''):
                        if (keyv[1] == 5):
                            rxGain = 38.8
                        elif (keyv[1] == 7):
                            rxGain = 39.5
                        else:
                            sys.exit('ERROR:  "freq found not in UNII-5 or UNII-7')

                        # 6 ft converted to m
                        csmap[keyv][ri][rxAntDiameterIdx] = 6*12*2.54/100

                    # R2-AIP-05 (a)
                    elif (float(r[rxAntDiameterIdx]) != -1):
                        rxGainULS = float(r[rxGainULSIdx])
                        Drx = float(r[rxAntDiameterIdx])
                        EtaRx = 0.55
                        Gtypical = 10*math.log10(EtaRx*(math.pi*Fc_unii*Drx/c)**2)
                        if (rxGainULS >= Gtypical - 0.7) and (rxGainULS <= Gtypical + 0.7):
                            rxGain = rxGainULS
                        elif (r[rxMidbandGainIdx].strip() != ''):
                            rxGain = float(r[rxMidbandGainIdx])
                        else:
                            rxGain = Gtypical

                    # R2-AIP-05 (b)
                    elif (float(r[txAntDiameterIdx]) != -1) and (r[txGainULSIdx].strip() != ''):
                        rxGainULS = float(r[rxGainULSIdx])
                        txGainULS = float(r[txGainULSIdx])
                        if (abs(rxGainULS - txGainULS) <= 0.1):
                            Drx = float(r[txAntDiameterIdx])
                            EtaRx = 0.55
                            Gtypical = 10*math.log10(EtaRx*(math.pi*Fc_unii*Drx/c)**2)
                            if (rxGainULS >= Gtypical - 0.7) and (rxGainULS <= Gtypical + 0.7):
                                rxGain = rxGainULS
                            elif (r[txMidbandGainIdx].strip() != ''):
                                rxGain = float(r[txMidbandGainIdx])
                            else:
                                rxGain = Gtypical
                            csmap[keyv][ri][rxAntDiameterIdx] = Drx

                    if (rxGain == ""):
                        matchFlagRxGain = True
                    else:
                        matchFlagRxGain = False

                    if float(r[rxHeightAGLIdx]) == -1:
                        matchFlagRxHeight = True
                    else:
                        matchFlagRxHeight = False

                    if float(r[txHeightAGLIdx]) == -1:
                        matchFlagTxHeight = True
                    else:
                        matchFlagTxHeight = False

                    if (matchFlagRxGain or matchFlagRxHeight or matchFlagTxHeight) and r[return_FSIDIdx].strip() == '':
                        rxLat = float(r[rxLatIdx])
                        rxLon = float(r[rxLonIdx])
                        txLat = float(r[txLatIdx])
                        txLon = float(r[txLonIdx])
                        foundMatch = False
                        foundCandidate = False
                        for (mi, m) in enumerate(csmap[keyv]):
                            if (not foundMatch) and (mi != ri):
                                m_rxLat = float(m[rxLatIdx])
                                m_rxLon = float(m[rxLonIdx])
                                m_txLat = float(m[txLatIdx])
                                m_txLon = float(m[txLonIdx])
                                m_txHeight = float(m[txHeightAGLIdx])
                                if (     (abs(rxLon-m_txLon) < 2.78e-4) and (abs(rxLat-m_txLat) < 2.78e-4)
                                     and (abs(txLon-m_rxLon) < 2.78e-4) and (abs(txLat-m_rxLat) < 2.78e-4) ):
                                    if m[return_FSIDIdx].strip() == '':
                                        foundMatch = True;
                                        matchi = mi
                                    else:
                                        foundCandidate = True;
                                        candidatei = mi
                        if foundMatch:
                            m = csmap[keyv][matchi]
                            csmap[keyv][ri][return_FSIDIdx] = m[FSIDIdx]
                            csmap[keyv][matchi][return_FSIDIdx] = r[FSIDIdx]
                            matchmap[ri] = matchi
                            matchmap[matchi] = ri
                            logFile.write("Matched FSID's: " + str(r[FSIDIdx]) + " and " + str(m[FSIDIdx]) + "\n")
                        elif foundCandidate:
                            m = csmap[keyv][candidatei]
                            csmap[keyv][ri][return_FSIDIdx] = '-' + m[FSIDIdx]
                            matchmap[ri] = candidatei
                            logFile.write("FSID: " + str(r[FSIDIdx]) + " using candidate match " + str(m[FSIDIdx]) + "\n")

                    matchi = matchmap[ri]

                    if matchFlagRxHeight:
                        csmap[keyv][ri][rxHeightAGLIdx] = '42.5'
                        if matchi != -1:
                            m = csmap[keyv][matchi]

                            if m[txHeightAGLIdx].strip() != '':
                                m_txHeightAGL = float(m[txHeightAGLIdx])
                                if m_txHeightAGL > 1.5:
                                    csmap[keyv][ri][rxHeightAGLIdx] = str(m_txHeightAGL)

                    if matchFlagTxHeight:
                        csmap[keyv][ri][txHeightAGLIdx] = '42.5'
                        if matchi != -1:
                            m = csmap[keyv][matchi]

                            if m[rxHeightAGLIdx].strip() != '':
                                m_rxHeightAGL = float(m[rxHeightAGLIdx])
                                if m_rxHeightAGL > 1.5:
                                    csmap[keyv][ri][txHeightAGLIdx] = str(m_rxHeightAGL)

                    # R2-AIP-05 (c)
                    if matchFlagRxGain:
                        if matchi != -1:
                            m = csmap[keyv][matchi]
                            if (float(m[txAntDiameterIdx]) != -1):

                                rxGainULS = float(r[rxGainULSIdx])
                                m_txGainULS = float(m[txGainULSIdx])
                                if (abs(rxGainULS - m_txGainULS) <= 0.1):
                                    Drx = float(m[txAntDiameterIdx])
                                    EtaRx = 0.55
                                    Gtypical = 10*math.log10(EtaRx*(math.pi*Fc_unii*Drx/c)**2)
                                    if (rxGainULS >= Gtypical - 0.7) and (rxGainULS <= Gtypical + 0.7):
                                        rxGain = rxGainULS
                                    elif (m[txMidbandGainIdx].strip() != ''):
                                        rxGain = float(m[txMidbandGainIdx])
                                    else:
                                        rxGain = Gtypical
                                    csmap[keyv][ri][rxAntDiameterIdx] = Drx

                    # R2-AIP-05 (d)
                    if rxGain == "":
                        rxGainULS = float(r[rxGainULSIdx])
                        if rxGainULS < 32.0:
                            rxGain = 32.0
                        elif rxGainULS > 48.0:
                            rxGain = 48.0
                        else:
                            rxGain = rxGainULS
                        oneOverSqrtEtaRx = 1.0/math.sqrt(0.55)
                        Drx = (c/(math.pi*Fc_unii))*(10**((rxGain)/20))*oneOverSqrtEtaRx
                        csmap[keyv][ri][rxAntDiameterIdx] = Drx

                    csmap[keyv][ri][rxGainIdx] = str(rxGain)
                    csmap[keyv][ri][txGainIdx] = r[txGainULSIdx]

                for r in csmap[keyv]:
                    csvwriter.writerow(r)
