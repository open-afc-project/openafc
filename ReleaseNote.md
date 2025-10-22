# Release Note

## **Version and Date**
|Version|**391*|
| :- | :- |
|**Date**|**10/01/2025**|

## **Issues Addressed**
 * 391: Remove usage of bitnami kafka docker image and other bitnami images

## **Interface Changes**
* None

## **Testing Done**
* Verified ALS logging was still being performed
* Regession tests to validate no other changes

|Version|**373*|
| :- | :- |
|**Date**|**07/15/2025**|

## **Issues Addressed**
 * 373: Update ULS parser to properly parse updated ISED's Stations Data Extract


## **Interface Changes**
 * Updated the uls-script C++ program to filter on both "Service" (=9) and "ITU Class of Station" (=RA) to consider a link to be a radio astronomy site (RAS).

## **Testing Done**
 * Reviewed the latest generated RAS database by ULS Parser and confirmed that only the one Canadian RAS link (per the latest Stations Data Extracts) is added and the other two link (Service=9, Subservice = TC) are not added to the FS or RAS database.

 * Ran four tests:
 * Test 1: a point over the first link (service=9, ITU Class of Station=TC, authorization number=011013568-002) that shouldn't be protected in Canada, requesting all frequencies and channels. Confirmed from results.kmz file that there are no RAS at the both this link and the second link that shouldn't be protected.
 * Test 2: a point over the same link as Test 1, requesting only the frequency channel over which the this link operates. Confirm confirmed that the channel is still used.
 * Test 3: repeat of Test 2 but for the second link(service=9, ITU Class of Station=TC, authorization number=011013632-002) that shouldn't be protected in Canada.
 * Test 4: a point at the center of the one RAS in Canada, requesting all frequencies and channels. Confirmed that the overlapping channels and frequencies are blocked.

## **Open Issues** 


|Version|1.0.1.0|
| :- | :- |
|**Date**|**10/28/2024**|
| tag 1.0.3.0|

## **Issues Addressed**
* 307. Rcache alembic migrations copy to docker image restored (#308)
* 300. Switch Python in Alpine images to venv (#301)
* 302. Fix last rcache upgrade script (#303)
* 288. Add keyhole support (#299)
* 295. PostGIS extension for newly created rcache database added (#296)
* 290. Fixing Alpine to 3.18.11, changing localhost to 127.0.0.1 in wget-based healtchecks (#293)
* 289. Migrating Rcache DB from latitude/longitude to PostGIS geometry (#292)
* Revert "281. AFC Enginr facelift (#282)" (#283)
* 281. AFC Enginr facelift (#282)
* 279. User creation added to database creation code (#280)
* 276. Database creation centralization (#277)
* Bump jinja2 from 3.1.4 to 3.1.5 in /cert_db (#268)
* 260. Grafana for compose environment (#273)
* 274. Resuscitation of FCC certificate downloading (#275)
* 271. Fixing use of $__file directives in Grafana-related Jinja templates (#272)
* 269. Adding timezones to ULS-related timetags (#270)
* 266. AFC traffic metrics (#267)
* adding variables for base and custom docker-compose files (#263)
* 264. ULS logging improvements (#265)
* Bump jinja2 from 3.1.2 to 3.1.4 in /grafana (#258)
* Unify and put to one place the docker-compose.yaml file (#242)
* 261. Fixing ULS download failure (#262)
* 257.  cert_db fixes and improvements (#259)
* 249. Further Grafana arrangements (#255)
* 251. Add IP and request flags to ALS (#253)
* 246. als_siphon improvements (#247)
* 243. Grafana relocation to top level (#244)

## **Version and Date**
|Version|**135*|
| :- | :- |
|**Date**|**11/12/2024**|

## **Issues Addressed**
 * 135: Country Boundary Fix 
 * Added proc_boundary functionality to proc_gdal (https://github.com/open-afc-project/openafc/tree/135-country-boundary-fix/tools/geo_converters/proc_gdal) and included proc_boundary_examples for USA and Canada.
 * Updated database_readme.md with 'Detailed Instructions for Downloading and Converting 3DEP Files For Use In AFC'and minor editorial changes

## **Interface Changes**
 * Only the country boundary (for US and Canada) kml files need to be updated. 
 * The updated kml files are attached to the issue 135 on Github

## **Testing Done**
 * Ran FSP1 on our local dev server and confirmed getting golden response.

## **Open Issues** 


|Version|**226*|
| :- | :- |
|**Date**|**11/02/2024**|

## **Issues Addressed**
 * 226: Update jquery version from 1.11.0 


## **Interface Changes**
 * There was an unneeded link to an old jQuery that was removed

## **Testing Done**
 * Exercised UI: Updated configuration, exercised web page controls, ran a request, all functionality seems working

## **Open Issues** 


|Version|1.0.1.0|
| :- | :- |
|**Date**|**10/28/2024**|
|compiled server's version is TBD | git tag 1.0.1.0|

## **Issues Addressed**
 *Issue 189. Gdal translation parameters ation improvement (#201)
 *Issue 202 .als_query.py bugfixes (#203)
 *Issue 175. als_query.py update (#196)
 *Issue 194. Generation of PSD and EIRP tables in ALS database (#195)
 *Issue 187. Typo fixed that prevented Rcache invalidation logging (#188)
 *Issue 190. ALS initialization added to afcerver (#191)
 *Issue 173. Adding ALS log to ULS downloads (#184)
 *Issue 182. Nodeport for webui added to rmq (#183)
 *Issue 180. Acknowledging worker-to-afcserver RMQ messages (#181)
 *Issue 178. External-secrets version fixed at v0.10.3 (#179)
 *Issue Add readme for indoor certificatin (#176)
 *Issue 169. proc_gdal made buildable (#170)
 *Issue 171. Renaming afc-server image to webui-image (#172)
 *Issue 167. Make build_imgs.sh fail on build failures (#177)
 *Issue (167-fail-build_imgs-on-build-failures) 165. Helm improvements (#166)
 *Issue 163. helm/README.md updated (#164)
 *Issue removing ./infra folder (#162)
 *Issue moving urllib3 to version 1.26.19 (#161)
 *Issue bumping versions of all python packages recommended by dependabot (#151)
 *Issue 156. Helm improvements (#158)
 *Issue 155. afc_load_tool.py improvements (#157)
 *Issue 152. 'run ngnx-rat_server-rmq-worker path tests' step added to github regression tests (#153)
 *Issue 142. CSRF protection by double cookie submission (#145)
 *Issue Bump requests from 2.31.0 to 2.32.2 in /dispatcher (#78)
 *Issue 2 migrated itm with building data propagation model is broken 837 (#143)
 *Issue adding static volume mount for objst storage folder (#147)
 *Issue 138. Adding pool_pre_ping=True SqlAlchemy engine creation parameter (#139)
 *Issue 134 additional update of database readme file (#137)
 *Issue 132. Afcserver-related bugfixes (#133)
 *Issue 130. Postpone RMQ connection until first AFC request (#131)
 *Issue 128. Precompute ordered in most-recent-updated-first (#129)
 *Issue 114. Afcserver - FastAPI implementation of msghnd (#125)
 *Issue 119. Мake Rcache shareable with afc server (#121)
 *Issue 123. Fix regression tests (#124)
 *Issue 122. Adding afcserver and sme improvements into helm/ (#126)
 *Issue 116. Extract DB/Rcache hardcoded stuff into modules, shareable with new msghnd implementation (#118)
 *Issue Added description of RASDatabase.dat to open-afc/src/README.md. (#120)
 *Issue 111 reimplement action public ip (#112)
 *Issue 105 fix exc thr file (#113)
 *Issue 108 update-public-ip-action-of-regr-tests (#109)
 *Issue 104 - Revert formatting changes (#107)
 *Issue 102 remove anomalous fs rxgain check (#103)
 *Issue 99. Increasing ingress-nginx timeout for msghnd (#100)
 *Issue 97. Add ALS  logging of rcache-related events (#98)
 *Issue 92. Adding pool_pre_ping=True to Rcache SqlAlchemy engine creation (#96)
 *Issue 94. Revising nginx settings for Grafana and Prometheus (#95)
 *Issue 85. No check for nginx process in acceptor.py (#92)
 *Issue 86. Renaming objstore healthcheck from /healthy to /objst_healthy (#91)
 *Issue 87. Changes in values.yaml (#90)
 *Issue 88.  Exposing Prometheus and Grafana on endpoints (#89)
 *Issue 83. Crash dispatcher if nginx not started (#84)
 *Issue 81. Placing dispatcher certificates back to same directory (#82)
 *Issue Adding prometheus parameters for test env (#80)
 *Issue 72. Fixing Kubernetes install problems and bugs (#76)
 *Issue 62. Add optional dispatcher passthroughs for Grafana and Prometheus (#74)
 *Issue (origin/77-deepdiff-is-incompatible-with-new-version-of-numpy) 69. Graceful shutdown enhancement (#70)
 *Issue 67. --timeout option added to afc_load_tool.py (#68)
 *Issue 65. Moving environment definitions from configmaps to deployment/statefulset helmcharts (#66)
 *Issue 61. Worker deadline (#64)
 *Issue 60. Moving ratafc accessto ratdb to scoped sessions (#63)
 *Issue 58. Moving some helm/bin scripts' command line parameters to YAML files (#59)
 *Issue 54 Regularize secrets (#57)
 *Issue 55 Stylistic and functional improvements in helmcharts (#56)
 *Issue 35. Adding GCP-related functionality to helm/ (#52)
 *Issue Fixed bug where integer calculation overflows 32 bits. (#50)
 *Issue 47. Fixing Prometheus metrics (#48)
 *Issue 45. afc_load_tool.py bugfixes and improvements (#46)
 *Issue 43. Fixed ALS Siphon build failure (#44)
 *Issue 32. Add secret support to helm (#33)
 *Issue 9. Restoring support of tiled NLCD in AFC engine (#30)
 *Issue 28. Adding k3d support to afc_load_tool.py (#29)
 *Issue 25 fix confluent kafka build (#27)
 *Issue 25. Fixing confluent-kafka client obtaining (#26)
 *Issue 22. Chapter on autoscaling added to README.md (#23)
 *Issue 5. k3d helmcharts and scripts. See README.md for gory details (#16)
 *Issue Adding minor change to test downstream fetching it. (#14)
 *Issue Update ReleaseNote.md with git hash for rel 1.0.0.0 (#13)

## **Version and Date**
|Version|**2&136*|
| :- | :- |
|**Date**|**08/28/2024**|

## **Issues Addressed**
 * 2: Migrated - 'ITM with building data' propagation model is broken (837)
 * 136: Update Canada default afc-config 

## **Interface Changes**
 * There were changes to the UI code to define default afc-config for Canada (per issue 136)

## **Testing Done**
 * 2: Ran a small test and confirmed that ITM with LiDAR is used as set in afc-config. The test configuration and result are attached to the issue 2.
 * 136: Confirmed that the default Canada afc-config is correct. This file is attached to issue 136 after the change.

## **Open Issues** 

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
