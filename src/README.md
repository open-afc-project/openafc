open-afc/src contains several modules/packages.

afc-engine: Performs the core computational analysis to determine channel availability.  The afc-engine reads a json configuration file and a json request file, and generates a json response file.

coalition_ulsprocessor: Takes raw FS data in the format provide by the FCC, or corresponding  government agency for other countries, and pieces together FS links.

rkflogging: Library used by afc-engine for creating run logs.

rkfsql: Library used by the afc-engine for reading sqlite database files.

ratapi: Python layer that provides the REST API for administration of the system and responding to requests

web: see src/web/README.md
