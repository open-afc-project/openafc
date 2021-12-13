import csv
import sys

csmap = {}

remMissTxEIRPGFlag = False
filterMaxEIRPFlag = False

def sortCallsigns(inputPath, outputPath):
    with open(inputPath, 'r') as f:
        with open(outputPath, 'w') as fout:
            csvreader = csv.reader(f, delimiter=',')
            csvwriter = csv.writer(fout, delimiter=',')
            firstRow = True
            for row in csvreader:
                if firstRow:
                    row.append("record")
                    row.append("lowest_dig_mod_rate")
                    row.append("highest_dig_mod_rate")
                    if filterMaxEIRPFlag:
                        row.append("highest_tx_eirp")
                    csvwriter.writerow(row)
                    firstRow = False
                    callsignIdx =  -1
                    pathIdx =  -1
                    freqIdx =  -1
                    emdegIdx =  -1
                    digitalModRateIdx =  -1
                    txEirpIdx =  -1
                    txGainIdx =  -1
                    for fieldIdx, field in enumerate(row):
                        if field == "Callsign":
                            callsignIdx = fieldIdx
                        elif field == "Path Number":
                            pathIdx = fieldIdx
                        elif field == "Center Frequency (MHz)":
                            freqIdx = fieldIdx
                        elif field == "Emissions Designator":
                            emdegIdx = fieldIdx
                        elif field == "Digital Mod Rate":
                            digitalModRateIdx = fieldIdx
                        elif field == "Tx EIRP (dBm)":
                            txEirpIdx = fieldIdx
                        elif field == "Tx Gain (dBi)":
                            txGainIdx = fieldIdx
                    if callsignIdx == -1:
                        sys.exit('ERROR: Callsign not found')
                    if pathIdx == -1:
                        sys.exit('ERROR: Path Number not found')
                    if freqIdx == -1:
                        sys.exit('ERROR: Center Frequency (MHz) not found')
                    if emdegIdx == -1:
                        sys.exit('ERROR: Emissions Designator not found')
                    if digitalModRateIdx == -1:
                        sys.exit('ERROR: Digital Mod Rate not found')
                    if txEirpIdx == -1:
                        sys.exit('ERROR: Tx EIRP (dBm) not found')
                    if txGainIdx == -1:
                        sys.exit('ERROR: Tx Gain (dBi) not found')

                    # print ("Callsign IDX = ", callsignIdx)
                    # print ("Path IDX = ", pathIdx)
                    # print ("Freq IDX = ", freqIdx)
                    # print ("Emdeg IDX = ", emdegIdx)
                    # print ("Digital Mod Rate IDX = ", digitalModRateIdx)
                    # print ("Tx EIRP (dBm) IDX = ", txEirpIdx)
                    # print ("Tx Gain (dBi) IDX = ", txGainIdx)
                else:
                    cs = row[callsignIdx]
                    path = int(row[pathIdx])
                    freq = row[freqIdx]
                    emdeg = row[emdegIdx][0:4]
                    if remMissTxEIRPGFlag and (row[txEirpIdx].strip() == '' or row[txGainIdx].strip() == ''):
                        print ("Removed entry missing Tx EIRP or Tx Gain")
                    else:
                        keyv = tuple([cs, path, freq, emdeg])
                        if keyv in csmap:
                            csmap[keyv].append(row)
                        else:
                            csmap[keyv] = [ row ]

            all_cs = csmap.keys()
            # all_cs.sort()
            for keyv in sorted(all_cs):
                rate_idx_list = []
                for (ri, r) in enumerate(csmap[keyv]):
                    if r[digitalModRateIdx].strip() == '':
                        rate = 0.0
                    else:
                        rate = float(r[digitalModRateIdx])
                    rate_idx = tuple([rate, ri])
                    rate_idx_list.append(rate_idx)
                rate_idx_list.sort()

                for (idx, rate_idx) in enumerate(rate_idx_list):
                    ri   = rate_idx[1]
                    r = csmap[keyv][ri]
                    if filterMaxEIRPFlag:
                        txEirp = float(r[txEirpIdx])
                        if idx == 0 or txEirp > maxEirp:
                            maxEirp = txEirp
                            maxEirpIdx = ri
                initFlag = True
                recordNum = 1
                for rate_idx in rate_idx_list:
                    rate = rate_idx[0]
                    ri   = rate_idx[1]
                    r = csmap[keyv][ri]
                    if rate == 0.0:
                        lowRateFlag = 2
                        highRateFlag = 2
                    else:
                        if initFlag:
                            lowRateFlag = 1
                            initFlag = False
                        else:
                            lowRateFlag = 0

                        if recordNum == len(rate_idx_list):
                            highRateFlag = 1
                        else:
                            highRateFlag = 0

                    r.append(str(recordNum))
                    r.append(str(lowRateFlag))
                    r.append(str(highRateFlag))
                    if filterMaxEIRPFlag:
                        if ri == maxEirpIdx:
                            printFlag = 1
                            r.append(str(1))
                        else:
                            printFlag = 0
                            r.append(str(0))
                    else:
                        if recordNum == 1:
                            printFlag = 1
                        else:
                            printFlag = 0

                    if printFlag:
                        csvwriter.writerow(r)

                    recordNum = recordNum + 1

