# DailyULSScript Readme

## **Assumptions**

* The C++ uls-processor executable likely needs to be rebuilt on your system.
** Source code can be found under /src/coalition_ulsprocessor
** To build, simply run: sh build.sh
** The resulting executable will be under /testroot/bin/uls-script
** uls-script executable **MUST** be moved to /var/lib/fbrat/daily\_uls\_parse


## **Usage**


* The scripts have been built into celery tasks are accessable via the API 
  * Celery Tasks can be found in /src/ratapi/ratapi/tasks/afc_worker
  * There are 3 new celery tasks to consider: 
  * A cron job that runs every minute to see if the automated ULS update should start
    * Currently it check a text file containing the next run time-- this can be overwritten by an admin in the GUI- and a celery config flag to see if a parse was already done that day.
  * A cron job that runs every day at midnight (UTC) that resets the automated parse flag 
    * This task at midnight will overwrite the config tag so that the next day it will be able to parse properly.
  * parseULS, which runs the scripts that download, parse, and produce the ULS DB
  * The first two tasks are configured automatically when the application starts. You can find that function [here](https://github.com/Telecominfraproject/open-afc/blob/2cf091084f9bf96fd121222d5476b2ff37eadca9/src/ratapi/ratapi/tasks/afc_worker.py#L34)
  * The third task (parseULS) is either run automatically by task 1 or it is run manually by a user in the admin tab.
    * To use it programmatically, see [this example](https://github.com/Telecominfraproject/open-afc/blob/2cf091084f9bf96fd121222d5476b2ff37eadca9/src/ratapi/ratapi/views/ratapi.py#L693) for the manual parse in the API. It applies the asynchronous task which returns the task. This task's task_id allows us to interact with the worker after resolving this API call and cancel parse in progress. A future feature may be to add a progress bar similar to how the engine analysis works in the GUI.
  * Note that for the cron job based tasks, if you want to interact or cancel the cron jobs programmatically then you'll need to keep track of the task ID that is associated with the tasks. [This documentation](https://docs.celeryproject.org/en/stable/userguide/periodic-tasks.html) on celery may prove useful.

* The API updates include:
  * Adding the parser and DB scripts under /src/ratapi/ratapi/db
  * Support for manually starting a parse
  * Updating the automated parse time
  * Getter for last successful parse time
* A csv of the database and a csv of the anomalous ULS records can also be downloaded
  * Look for a zip file with the corresponding date in the name under ULS Databases


## *Potential Improvements*

* Do bulk lookups or equivalent SQL operation for finding if FSID(s) already exist
  * Currently, uls records are processed one by one to look up the FSID in the old database
  * This is by far the most time consuming operation and takes over an hour as there are >100k entries in a ULS
    * They are selected based on 4 fields that produce a unique record: callsign, freq\_assigned\_start\_mhz, freq
_assigned\_end\_mhz, path\_number
    * Composite primary keys of these fields may work, but FSID must also be unique (currently it's the primary key)

* If improved runtime is desired then RKF would recommend implementing multithreading
  * There are at least two specific cases where multithreading would prove beneficial

1. Processing the files for a specific day on separate threads
   * If you do this you MUST ensure that you wait for each day to finish before progressing to the next day
   * For example let's say the file we are considering is Monday
       * Each day has the following files to consider: AN.dat CP.dat EM.dat EN.dat FR.dat HD.dat LO.dat PA.dat SG.dat
       * We can process each one of Mondays files its own thread but we CANNOT process Tuesdays files until the same file for Monday is finished

2. Do the file downloads in parallel
   * Currently downloads 1 by 1
   * Weekly file is much bigger than others, so this won&#39;t be a huge speedup