#!/usr/bin/env python3
#
# Copyright (C) 2021 Broadcom. All rights reserved. The term "Broadcom"
# refers solely to the Broadcom Inc. corporate affiliate that owns
# the software below. This work is licensed under the OpenAFC Project License,
# a copy of which is included with this software program
#

"""Downloader and archive extraction module for FCC ULS and Canadian datasets."""

import hashlib
from collections import OrderedDict
import os
import shutil
import ssl
import urllib.error
import urllib.parse
import urllib.request
import zipfile

ssl._create_default_https_context = ssl.create_default_context

# Decompression bounds for FCC ULS archives. The weekly
# l_micro.zip expands to well under 5 GiB; reject archives
# whose declared total size or per-member ratio exceeds safe limits.
ULS_ZIP_MAX_BYTES = 20 * 1024 * 1024 * 1024
ULS_ZIP_MAX_RATIO = 100

# Map to reuse weekday in loops
dayMap = OrderedDict()
dayMap[6] = 'sun'
dayMap[0] = 'mon'
dayMap[1] = 'tue'
dayMap[2] = 'wed'
dayMap[3] = 'thu'
dayMap[4] = 'fri'
dayMap[5] = 'sat'


class _MD5RedirectHandler(urllib.request.HTTPRedirectHandler):
    """Custom redirect handler to prevent following help/search landing pages."""

    def redirect_request(self, req, fp, code, msg, headers, newurl):
        if 'what-can-we-help-you-find' in newurl or 'not-found' in newurl:
            return None
        return super().redirect_request(req, fp, code, msg, headers, newurl)


def safe_download(url, filename, md5_url=None):
    """Download a file with size bounds and optional publisher-supplied MD5 check."""
    if not url.startswith('http://') and not url.startswith('https://'):
        raise ValueError("Invalid URL scheme")

    def _cap(block_num, block_size, total_size):
        if block_num * block_size > ULS_ZIP_MAX_BYTES:
            raise RuntimeError(
                'Rejecting %s: download exceeds cap (%d bytes)'
                % (url, ULS_ZIP_MAX_BYTES))

    urllib.request.urlretrieve(url, filename, reporthook=_cap)

    # Verify input artifact digest when provided
    if md5_url is not None:
        if not md5_url.startswith('https://'):
            raise ValueError("Invalid MD5 URL scheme")

        def _cleanup_and_raise(err):
            try:
                os.remove(filename)
            except OSError:
                pass
            raise RuntimeError(
                'Rejecting %s: input-artifact MD5 verification failed '
                '(%s) -- refusing to feed unverified bytes to uls-script'
                % (url, err))

        try:
            opener = urllib.request.build_opener(_MD5RedirectHandler)
            with opener.open(md5_url, timeout=60) as r:
                if 'what-can-we-help-you-find' in r.geturl() or 'text/html' in r.headers.get('Content-Type', ''):
                    raise urllib.error.HTTPError(r.geturl(), 404, 'MD5 file not published', r.headers, None)
                expected = r.read(4096).decode('ascii', 'strict').split()[0]
            if len(expected) != 32:
                raise ValueError('malformed digest')
            int(expected, 16)
            h = hashlib.md5(usedforsecurity=False)  # Non-cryptographic integrity check
            with open(filename, 'rb') as f:
                for chunk in iter(lambda: f.read(1 << 20), b''):
                    h.update(chunk)
            if h.hexdigest().lower() != expected.lower():
                raise ValueError('digest mismatch')
        except urllib.error.HTTPError as e:
            if e.code in (404, 301, 302, 303, 307, 308) or (e.code == 403 and 'fcc.gov' in md5_url):
                print(
                    'WARNING: MD5 file not published by server for %s (HTTP %s) '
                    '-- skipping MD5 check; rely on ULS_HASH_MANIFEST for '
                    'output-artifact integrity' % (url, e.code))
            else:
                _cleanup_and_raise(e)
        except Exception as e:
            _cleanup_and_raise(e)


def downloadFiles(region, logFile, currentWeekday, fullPathTempDir):
    """Download data files for each region (US, CA) into temp directory."""
    regionDataDir = fullPathTempDir + '/' + region
    if not os.path.isdir(regionDataDir):
        os.mkdir(regionDataDir)
    logFile.write('Downloading data files for ' +
                  region + ' into ' + regionDataDir + '\n')
    if region == 'US':
        # Download the latest Weekly Update
        weeklyURL = 'https://data.fcc.gov/download/pub/uls/complete/l_micro.zip'
        logFile.write('Downloading weekly' + '\n')
        safe_download(weeklyURL, regionDataDir + '/weekly.zip',
                      md5_url=weeklyURL + '.md5')

        # Download all the daily updates starting from Sunday up to currentWeekday
        for key, day in dayMap.items():
            dayStr = day
            dailyURL = 'https://data.fcc.gov/download/pub/uls/daily/l_mw_' + dayStr + '.zip'
            logFile.write('Downloading ' + dayStr + '\n')
            safe_download(
                dailyURL, regionDataDir + '/' + dayStr + '.zip',
                md5_url=dailyURL + '.md5')
            # Exit after processing today's file
            if (key == currentWeekday) and (day != 'sun'):
                break
    elif region == 'CA':
        safe_download(
            'https://www.ic.gc.ca/engineering/Stations_Data_Extracts.csv',
            regionDataDir + '/SD.csv')
        safe_download(
            'https://www.ic.gc.ca/engineering/Passive_Repeater_data_extract.csv',
            regionDataDir + '/PP.csv')
        safe_download(
            'https://www.ic.gc.ca/engineering/Passive_Reflectors_Data_Extract.csv',
            regionDataDir + '/PR.csv')

        with open(os.path.join(regionDataDir, 'AP.csv'), 'w') as f:
            f.write(
                "Antenna Manufacturer,Antenna Model Number,Antenna Gain [dBi],"
                "Antenna Diameter,Beamwidth [deg],Last Updated,Pattern Type,"
                "Pattern Azimuth [deg],Pattern Attenuation [dB]\n"
            )
        safe_download(
            'https://www.ic.gc.ca/engineering/Antenna_Patterns_6GHz.csv',
            regionDataDir + '/Antenna_Patterns_6GHz_orig.csv')
        with open(os.path.join(regionDataDir, 'Antenna_Patterns_6GHz_orig.csv'), 'r') as src:
            with open(os.path.join(regionDataDir, 'AP.csv'), 'a') as dst:
                for chunk in iter(lambda: src.read(8192), ''):
                    dst.write(chunk)
        os.remove(regionDataDir + '/Antenna_Patterns_6GHz_orig.csv')


def getMostRecentRegionDownload(region, fullPathSaveDir):
    """Retrieve the path to the most recent saved download for the region."""
    regionSaveParentDir = os.path.join(fullPathSaveDir, region)
    if not os.path.isdir(regionSaveParentDir):
        return None

    existingBackups = sorted(
        d for d in os.listdir(regionSaveParentDir)
        if os.path.isdir(os.path.join(regionSaveParentDir, d))
    )
    return os.path.join(regionSaveParentDir, existingBackups[-1]) if existingBackups else None


def replaceRegionDataWithLastSuccess(region, fullPathSaveDir, fullPathTempDir):
    """Replace the region data with the most recent backup found in save directory."""
    destDir = os.path.join(fullPathTempDir, region)
    srcDir = getMostRecentRegionDownload(region, fullPathSaveDir)
    if srcDir is None:
        raise FileNotFoundError(f"Backup directory does not exist for region {region}.")
    if os.path.exists(destDir):
        shutil.rmtree(destDir)
    shutil.copytree(srcDir, destDir)


def handleRegionFailure(region, regionFailureFlag, fullPathSaveDir, fullPathTempDir):
    """Handle a download failure for a region by falling back to last successful backup."""
    if not regionFailureFlag:
        print(f"Attempting to replace the failed download of region: {region} with last success.")
        replaceRegionDataWithLastSuccess(region, fullPathSaveDir, fullPathTempDir)
        print("Replacement Successful.")
        return True
    else:
        raise RuntimeError("Previously successful data has failed.")


def prepareAFCGitHubFiles(rawDir, destDir, logFile):
    """Prepare and sanitize common data files from WinnForum repository."""
    dataFileList = [
        'antenna_model_diameter_gain.csv',
        'billboard_reflector.csv',
        'category_b1_antennas.csv',
        'high_performance_antennas.csv',
        'fcc_fixed_service_channelization.csv',
        'transmit_radio_unit_architecture.csv',
    ]

    for dataFile in dataFileList:
        srcFile = os.path.join(rawDir, dataFile)
        dstFile = os.path.join(destDir, dataFile)
        with open(srcFile, 'rb') as f_in, open(dstFile, 'wb') as f_out:
            for chunk in iter(lambda: f_in.read(8192), b''):
                f_out.write(chunk.translate(None, bytes(range(128, 256)) + b'\r'))
        if dataFile == "fcc_fixed_service_channelization.csv":
            with open(dstFile, 'a') as f_out:
                f_out.write("5967.4375,30,\n6056.3875,30,\n6189.8275,30,\n6219.4775,30,\n6308.4275,30,\n")


def extractZips(logFile, directory):
    """Extract zip archives with decompression ratio and size limits."""
    logFile.write('Extracting zips for directory ' + directory + '\n')
    for tempZip in os.listdir(directory):
        if tempZip.endswith('.zip'):
            logFile.write('Extracting ' + tempZip + '\n')
            fileName = os.path.abspath(directory + '/' + tempZip)
            zip_file = zipfile.ZipFile(fileName)
            total_out = 0
            for zi in zip_file.infolist():
                ratio = zi.file_size / max(zi.compress_size, 1)
                total_out += zi.file_size
                if ratio > ULS_ZIP_MAX_RATIO or total_out > ULS_ZIP_MAX_BYTES:
                    zip_file.close()
                    raise RuntimeError(
                        'Rejecting %s: decompressed size %d / ratio %.1f '
                        'exceeds cap (%d bytes / %d:1)'
                        % (tempZip, total_out, ratio,
                           ULS_ZIP_MAX_BYTES, ULS_ZIP_MAX_RATIO))
            subDirName = fileName.replace('.zip', '')
            os.mkdir(subDirName)
            zip_file.extractall(subDirName)
            zip_file.close()
