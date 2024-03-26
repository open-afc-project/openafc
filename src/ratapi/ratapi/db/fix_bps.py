#!/usr/bin/python

import csv
import sys
import math

modmapBPS = {}
cslistBPS = {}


def RepresentInt(s):
    try:
        int(s)
        return True
    except BaseException:
        return False


def RepresentFloat(s):
    try:
        float(s)
        return True
    except BaseException:
        return False


def getBPSFromMod(mod_type, cs):
    try:
        bits_per_sym = int(modmapBPS[mod_type])
        if (len(cslistBPS[mc]) > 0):
            if cs not in cslistBPS[mc]:
                bits_per_sym = -1
        return bits_per_sym
    except BaseException:
        return -1


def getEmdegBW(emdeg):
    chList = ["H", "K", "M", "G", "T"]
    ba = bytearray(bytes(emdeg, encoding='utf8'))
    found = False
    sVal = 1.0
    for (chIdx, ch) in enumerate(chList):
        if not found:
            strpos = ba.find(bytes(ch, encoding='utf8'))
            if strpos != -1:
                ba[strpos] = ord('.')
                scale = sVal
                found = True

            sVal *= 1000.0
    bandwidth = float(ba) * scale
    return bandwidth


def fixBPS(inputPath, modcodFile, outputPath):
    with open(modcodFile, 'r') as f:
        for ln in f:
            lns = ln.strip().split(',')
            mc = lns.pop(0)
            bps = lns.pop(0)
            modmapBPS[mc] = bps
            cslistBPS[mc] = []
            for cs in lns:
                cslistBPS[mc].append(cs)

    csmap = {}

    modmapEmdegScale = {}
    modmapEmdegScale["M"] = 1.0e6

    with open(inputPath, 'r') as f:
        with open(outputPath, 'w') as fout:
            csvreader = csv.reader(f, delimiter=',')
            csvwriter = csv.writer(fout, delimiter=',')
            firstRow = True
            linenum = 0
            for row in csvreader:
                linenum = linenum + 1
                if firstRow:
                    row.append("mod_rate (BPS)")
                    row.append("spectral_efficiency (bit/Hz)")
                    row.append("comment")
                    csvwriter.writerow(row)
                    firstRow = False
                    callsignIdx = -1
                    pathIdx = -1
                    freqIdx = -1
                    emdegIdx = -1
                    digitalModRateIdx = -1
                    modTypeIdx = -1
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
                        elif field == "Digital Mod Type":
                            modTypeIdx = fieldIdx
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
                    if modTypeIdx == -1:
                        sys.exit('ERROR: Digital Mod Type not found')
                else:
                    cs = row[callsignIdx]
                    path = int(row[pathIdx])
                    freq = row[freqIdx]
                    emdeg = row[emdegIdx][0:4]
                    mod_type = row[modTypeIdx]
                    mod_rate_bps = ""
                    spectral_efficiency = ""
                    comment = ""
                    processFlag = True
                    removeFlag = False
                    mod_type = mod_type.replace(',', '')

                    if RepresentFloat(row[digitalModRateIdx]):
                        mod_rate = float(row[digitalModRateIdx])
                        if mod_rate == 0.0:
                            comment = "INVALID Digital Mod Rate = " + \
                                str(mod_rate)
                            # print ("WARN: "+ comment + " for callsign " + cs + " at line "+ str(linenum))
                            processFlag = False
                    else:
                        processFlag = False

                    # if "TCM" in mod_type or "VSB" in mod_type or "FM" in mod_type:
                    #     print linenum, "Removed line, mod_type = " + mod_type
                    #     removeFlag = True

                    bits_per_sym = getBPSFromMod(mod_type, cs)
                    if bits_per_sym == -1:
                        comment = "INVALID mod_type = " + mod_type
                        # print ("WARN: "+ comment + " for callsign " + cs + " at line "+ str(linenum))
                        processFlag = False

                    if emdeg == "":
                        processFlag = False

                    if processFlag:
                        bw = getEmdegBW(emdeg)

                        se = mod_rate / bw
                        if se == 0.0:
                            comment = "ERROR: se = 0.0, bw = " + \
                                str(bw) + ", mod_rate = " + str(mod_rate)
                            print(comment + " for callsign " +
                                  cs + " at line " + str(linenum))
                        scale = bits_per_sym / se
                        scaleFactor = 1.0
                        if scale > 1.0:
                            while scale > 10.0:
                                scaleFactor = scaleFactor * 10
                                scale = scale / 10
                        else:
                            while scale < 1.0:
                                scaleFactor = scaleFactor / 10
                                scale = scale * 10

                        mod_rate_bps = mod_rate * scaleFactor
                        spectral_efficiency = mod_rate_bps / bw

                    if not removeFlag:
                        row.append(mod_rate_bps)
                        row.append(spectral_efficiency)
                        row.append(comment)
                        csvwriter.writerow(row)
