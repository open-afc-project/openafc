# Release Note

## **Version and Date**
|Version|**105*|
| :- | :- |
|**Date**|**07/10/2024**|

## **Issues Addressed**
 * 105: fix exc_thr file


## **Interface Changes**
 * None

## **Testing Done**
 * Ran a test [FSP1 but for 20 MHz channels only] (see attached afc-config, request and response json files and exc_thr files to the issue) and validated that the updated exc_thr would now show the links with distance > 1km using ITM (rather the FSPL that was tried originally to determine whether the regulatory threshold is exceeded or not) (previously, only the FSPL ones were shown). When printSkippedLinks flag in AFC Config is set, links that pass the regulatory threshold with FSPL are shown as well. Note that when the printSkippedLinks flag is set to false, links that should be using FSPL (i.e. 30m distance or inside AP uncertainty footprint) are still shown.

## **Open Issues** 
 * Impact on speed from these changes need to be evaluated.

## **Version and Date**
|Version|**104*|
| :- | :- |
|**Date**|**07/08/2024**|

## **Issues Addressed**
Reverted the formatting of AfcManager.cpp to previous state due to overly agressive reformatting from clang-format

## **Version and Date**
|Version|**102*|
| :- | :- |
|**Date**|**02/29/2024**|

## **Issues Addressed**
 * 102: remove-anomalous-fs-rxgain-check
 * Fixing anomalous FS Rx Gain should be all done by the FS parser during generation of the FS database sqlite3 file. Recently, a bug was found where the afc-engine removed Canadian FS links with FS Rx Gain < 10 dBi. We hadn't seen this in the US AFC since the FS parser implements WinnForum TS 1014 R2-AIP-05 and 06 that ensures FS Rx Gain is at least 32 dBi. However, for Canada, the FS database is assumed to be sanitized already and minimum fix is done by the FS Parser (only if the gain is missing or 0). This check by the afc-engine has now been removed.

## **Interface Changes**
 * None

## **Testing Done**
 * Ran a test (see attached afc-config, request and response json files to the issue) and confirmed that the fs_anom.csv file is empty as expected. Previously, this file contained 23 links for this test that were removed due to having Rx Gain of 6 dBi).

## **Open Issues** 
 * None

## **Version and Date**
|Version|1.0.0.0|
| :- | :- |
|**Date**|**04/03/2024**|
|compiled server's version is c77f7cb | git tag 1.0.0.0|

## **Issues Addressed**
 * Issue 7: Kubernetes-related changes
 * Initial commit
