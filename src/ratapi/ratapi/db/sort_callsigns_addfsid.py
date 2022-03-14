import csv
import sys
from os.path import exists

csmap = {}
fsidmap = {}

remMissTxEIRPGFlag = False
filterMaxEIRPFlag = False

def sortCallsignsAddFSID(inputPath, fsidTableFile, outputPath):
    if not exists(fsidTableFile):
        print("FSIDTable does NOT exist, creating Table at " + fsidTableFile + "\n")
        with open(fsidTableFile, 'w') as fsidTable:
            fsidTable.write("FSID,Callsign,Path Number,Center Frequency (MHz),Emissions Designator\n")

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
                callsignIdx =  -1
                pathIdx =  -1
                freqIdx =  -1
                emdegIdx =  -1
                for fieldIdx, field in enumerate(row):
                    if field == "FSID":
                        fsidIdx = fieldIdx
                    elif field == "Callsign":
                        callsignIdx = fieldIdx
                    elif field == "Path Number":
                        pathIdx = fieldIdx
                    elif field == "Center Frequency (MHz)":
                        freqIdx = fieldIdx
                    elif field == "Emissions Designator":
                        emdegIdx = fieldIdx
                if fsidIdx != 0:
                    sys.exit('ERROR: Invalid FSID table file, FSID not found')
                if callsignIdx != 1:
                    sys.exit('ERROR: Invalid FSID table file, callsign not found')
                if pathIdx != 2:
                    sys.exit('ERROR: Invalid FSID table file, path Number not found')
                if freqIdx != 3:
                    sys.exit('ERROR: Invalid FSID table file, Center Frequency (MHz) not found')
                if emdegIdx != 4:
                    sys.exit('ERROR: Invalid FSID table file, emissions Designator not found')
            else:
                fsid = int(row[fsidIdx])
                cs = row[callsignIdx]
                path = int(row[pathIdx])
                freq = row[freqIdx]
                emdeg = row[emdegIdx][0:4]
                keyv = tuple([cs, path, freq, emdeg])
                fsidmap[keyv] = fsid
                if firstFSID or fsid > highestFSID:
                    highestFSID = fsid
                firstFSID = False
                entriesRead += 1

    print("Read " + str(entriesRead) + " entries from FSID table file: " + fsidTableFile + ", Max FSID = " + str(highestFSID) + "\n")

    entriesAdded = 0
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

                        row.insert(0, "FSID")
                        csvwriter.writerow(row)
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
                            if keyv in fsidmap:
                                fsid = fsidmap[keyv]
                            else:
                                fsid = nextFSID
                                nextFSID += 1
                                fsidTable.write(str(fsid) + "," + keyv[0] + "," + str(keyv[1]) + "," + keyv[2] + "," + keyv[3] + "\n")
                                entriesAdded += 1
                            r.insert(0, str(fsid))
                            csvwriter.writerow(r)
    
                        recordNum = recordNum + 1

    print("Added " + str(entriesAdded) + " entries to FSID table file: " + fsidTableFile + "\n")
