import csv
import sys
from os.path import exists

csmapA = {} # Items stored in fsid table (currently FS in US)
csmapB = {} # Items not stored in fsid table (currently FS in CA)
fsidmap = {}

remMissTxEIRPGFlag = False
filterMaxEIRPFlag = False

def sortCallsignsAddFSID(inputPath, fsidTableFile, outputPath, logFile):
    logFile.write('Sorting callsigns and adding FSID' + '\n')
    if not exists(fsidTableFile):
        logFile.write("FSIDTable does NOT exist, creating Table at " + fsidTableFile + "\n")
        with open(fsidTableFile, 'w') as fsidTable:
            fsidTable.write("FSID,Region,US,Callsign,Path Number,Center Frequency (MHz),Bandwidth (MHz)\n")

    entriesRead = 0
    highestFSID = 0
    with open(fsidTableFile, 'r') as fsidTable:
        csvreaderFSIDTable = csv.reader(fsidTable, delimiter=',')
        firstRow = True
        firstFSID = True
        for row in csvreaderFSIDTable:
            if firstRow:
                firstRow = False
                fsidIdx =  -1
                regionIdx =  -1
                callsignIdx =  -1
                pathIdx =  -1
                freqIdx =  -1
                bandwidthIdx =  -1
                for fieldIdx, field in enumerate(row):
                    if field == "FSID":
                        fsidIdx = fieldIdx
                    elif field == "Region":
                        regionIdx = fieldIdx
                    elif field == "Callsign":
                        callsignIdx = fieldIdx
                    elif field == "Path Number":
                        pathIdx = fieldIdx
                    elif field == "Center Frequency (MHz)":
                        freqIdx = fieldIdx
                    elif field == "Bandwidth (MHz)":
                        bandwidthIdx = fieldIdx
                if fsidIdx == -1:
                    sys.exit('ERROR: Invalid FSID table file, FSID not found')
                if regionIdx == -1:
                    sys.exit('ERROR: Invalid FSID table file, Region not found')
                if callsignIdx == -1:
                    sys.exit('ERROR: Invalid FSID table file, callsign not found')
                if pathIdx == -1:
                    sys.exit('ERROR: Invalid FSID table file, path Number not found')
                if freqIdx == -1:
                    sys.exit('ERROR: Invalid FSID table file, Center Frequency (MHz) not found')
                if bandwidthIdx == -1:
                    sys.exit('ERROR: Invalid FSID table file, Bandwidth (MHz) not found')
            else:
                fsid = int(row[fsidIdx])
                region = row[regionIdx]
                cs = row[callsignIdx]
                path = int(row[pathIdx])
                freq = float(row[freqIdx])
                bandwidth = float(row[bandwidthIdx])
                keyv = tuple([region, cs, path, freq, bandwidth])
                fsidmap[keyv] = fsid
                if firstFSID or fsid > highestFSID:
                    highestFSID = fsid
                firstFSID = False
                entriesRead += 1

    logFile.write("Read " + str(entriesRead) + " entries from FSID table file: " + fsidTableFile + ", Max FSID = " + str(highestFSID) + "\n")

    entriesAdded = 0
    count = 0;
    with open(inputPath, 'r') as f:
        with open(outputPath, 'w') as fout:
            with open(fsidTableFile, 'a+') as fsidTable:
                csvreader = csv.reader(f, delimiter=',')
                csvwriter = csv.writer(fout, delimiter=',')
                nextFSID = highestFSID + 1
                firstRow = True
                for row in csvreader:
                    if firstRow:
                        row.append("record")
                        row.append("lowest_dig_mod_rate")
                        row.append("highest_dig_mod_rate")
                        if filterMaxEIRPFlag:
                            row.append("highest_tx_eirp")
                        firstRow = False
                        regionIdx =  -1
                        callsignIdx =  -1
                        pathIdx =  -1
                        freqIdx =  -1
                        emdegIdx =  -1
                        digitalModRateIdx =  -1
                        txEirpIdx =  -1
                        txGainIdx =  -1
                        for fieldIdx, field in enumerate(row):
                            if field == "Region":
                                regionIdx = fieldIdx
                            elif field == "Callsign":
                                callsignIdx = fieldIdx
                            elif field == "Path Number":
                                pathIdx = fieldIdx
                            elif field == "Center Frequency (MHz)":
                                freqIdx = fieldIdx
                            elif field == "Bandwidth (MHz)":
                                bandwidthIdx = fieldIdx
                            elif field == "Digital Mod Rate":
                                digitalModRateIdx = fieldIdx
                            elif field == "Tx EIRP (dBm)":
                                txEirpIdx = fieldIdx
                            elif field == "Tx Gain (dBi)":
                                txGainIdx = fieldIdx
                        if regionIdx == -1:
                            sys.exit('ERROR: Region not found')
                        if callsignIdx == -1:
                            sys.exit('ERROR: Callsign not found')
                        if pathIdx == -1:
                            sys.exit('ERROR: Path Number not found')
                        if freqIdx == -1:
                            sys.exit('ERROR: Center Frequency (MHz) not found')
                        if bandwidthIdx == -1:
                            sys.exit('ERROR: Bandwidth (MHz) not found')
                        if digitalModRateIdx == -1:
                            sys.exit('ERROR: Digital Mod Rate not found')
                        if txEirpIdx == -1:
                            sys.exit('ERROR: Tx EIRP (dBm) not found')
                        if remMissTxEIRPGFlag and (txGainIdx == -1):
                            sys.exit('ERROR: Tx Gain (dBi) not found')

                        row.insert(0, "FSID")
                        csvwriter.writerow(row)
                    else:
                        region = row[regionIdx]
                        cs = row[callsignIdx]
                        path = int(row[pathIdx])
                        freq = float(row[freqIdx])
                        bandwidth = float(row[bandwidthIdx])
                        if remMissTxEIRPGFlag and (row[txEirpIdx].strip() == '' or row[txGainIdx].strip() == ''):
                            logFile.write("Removed entry missing Tx EIRP or Tx Gain")
                        else:
                            if region == 'US':
                                keyv = tuple([region, cs, path, freq, bandwidth])
                                if keyv in csmapA:
                                    csmapA[keyv].append(row)
                                else:
                                    csmapA[keyv] = [ row ]
                            else:
                                # For CA, dont remove any links, make keys unique for each link
                                keyv = tuple([region, count])
                                if keyv in csmapB:
                                    sys.exit('ERROR: Invalid key')
                                else:
                                    csmapB[keyv] = [ row ]
                    count+=1

                for typeIdx in range(2):
                    if typeIdx == 0:
                        all_cs = csmapA.keys()
                    elif typeIdx == 1:
                        all_cs = csmapB.keys()
                    else:
                        sys.exit('ERROR: Invalid typeIdx')

                    for keyv in sorted(all_cs):
                        rate_idx_list = []
                        if typeIdx == 0:
                            for (ri, r) in enumerate(csmapA[keyv]):
                                if r[digitalModRateIdx].strip() == '':
                                    rate = 0.0
                                else:
                                    rate = float(r[digitalModRateIdx])
                                rate_idx = tuple([rate, ri])
                                rate_idx_list.append(rate_idx)
                        else:
                            for (ri, r) in enumerate(csmapB[keyv]):
                                if r[digitalModRateIdx].strip() == '':
                                    rate = 0.0
                                else:
                                    rate = float(r[digitalModRateIdx])
                                rate_idx = tuple([rate, ri])
                                rate_idx_list.append(rate_idx)

                        rate_idx_list.sort()
    
                        for (idx, rate_idx) in enumerate(rate_idx_list):
                            ri   = rate_idx[1]
                            if typeIdx == 0:
                                r = csmapA[keyv][ri]
                            else:
                                r = csmapB[keyv][ri]
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
                            if typeIdx == 0:
                                r = csmapA[keyv][ri]
                            else:
                                r = csmapB[keyv][ri]
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
                                if recordNum == len(rate_idx_list):
                                    printFlag = 1
                                else:
                                    printFlag = 0
    
                            if printFlag:
                                if typeIdx == 0:
                                    if keyv in fsidmap:
                                        fsid = fsidmap[keyv]
                                    else:
                                        fsid = nextFSID
                                        nextFSID += 1
                                        fsidTable.write(str(fsid) + "," + keyv[0] + "," + keyv[1] + "," + str(keyv[2]) + "," + str(keyv[3]) + "," + str(keyv[4]) + "\n")
                                        entriesAdded += 1
                                else:
                                    fsid = nextFSID
                                    nextFSID += 1
                                r.insert(0, str(fsid))
                                csvwriter.writerow(r)
    
                            recordNum = recordNum + 1

    logFile.write("Added " + str(entriesAdded) + " entries to FSID table file: " + fsidTableFile + "\n")
