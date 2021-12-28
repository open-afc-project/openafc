#include "UlsFileReader.h"
#include <QtCore>
#include <limits>
#include <math.h>

namespace {
double emptyAtof(const char *str) {
  if (strlen(str) == 0)
    return std::numeric_limits<double>::quiet_NaN();
  else
    return atof(str);
}


//NOTE: Below function commented out becuase issue was resolved by increasing maxcol per the documentation. May want to update in future to use this approach instead of manual updates.
// Seeks forward in the file until a new line is found or EOF.
// Used to avoid any offseting issues in files that exit their loop because of maxcol.
void SetToNextLine(FILE *fi) {
  return;
  // char nextChar = fgetc(fi); 
  // while(nextChar != '\n' && nextChar != EOF) {
  //   nextChar = fgetc(fi);
  // }

}
}; // namespace

UlsFileReader::UlsFileReader(const char *fpath) {
  FILE *fi = fopen(fpath, "r");

  char front[3];

  QMap<QString, int> autoCellCounts;
  while (feof(fi) == 0) {
    front[0] = fgetc(fi);
    front[1] = fgetc(fi);
    front[2] = '\0';
    // qDebug() << front;
    if (front[0] == '|' || front[1] == '|')
      break;
    if (strcmp(front, "HD") == 0) {
      readIndividualHeader(fi);
    } else if (strcmp(front, "PA") == 0) {
      readIndividualPath(fi);
    } else if (strcmp(front, "AN") == 0) {
      readIndividualAntenna(fi);
    } else if (strcmp(front, "FR") == 0) {
      readIndividualFrequency(fi);
    } else if (strcmp(front, "LO") == 0) {
      readIndividualLocation(fi);
    } else if (strcmp(front, "EM") == 0) {
      readIndividualEmission(fi);
    } else if (strcmp(front, "EN") == 0) {
      readIndividualEntity(fi);
    } else if (strcmp(front, "MF") == 0) {
      readIndividualMarketFrequency(fi);
    } else if (strcmp(front, "CP") == 0) {
      readIndividualControlPoint(fi);
    } else if (strcmp(front, "SG") == 0) {
      readIndividualSegment(fi);
    } else {
      
      QString f(front);
      if (autoCellCounts.contains(f) == false) {
        /// Assume the first entry has the right number of cells for this type.
        int cnt = 0;
        do {
          int c = fgetc(fi);
          if (c == '|')
            cnt++;
          if (c == '\n')
            break;
          if (feof(fi))
            break;
        } while (true);
        autoCellCounts[f] = cnt;
      } else {
        /// Read the first 'n' pipes, then a '\n'.
        int n = autoCellCounts[f];
        do {
          int c = fgetc(fi);
          if (c == '|')
            n--;
          if (c == '\n') {
            if (n == 0)
              break;
          }
          if (feof(fi))
            break;
        } while (true);
      }
      continue;
    }

    /*int f = fgetc(fi);
    qDebug() << "read" << int(f) << ((f < 32)?'?':char(f));
    if(f == '\n') continue;
    if(f == '\r'){
        f = fgetc(fi);
        qDebug() << "read" << int(f);
        if(f == '\n') continue;
        else{
            qDebug() << "Unexpected character...";
            continue;
        }
        } */
  }
}

void UlsFileReader::readIndividualPath(FILE *fi) {
  char string[1024];

  //  Read in the string.
  char done = 0;
  int pos = 0;
  int numcol = 0;
  UlsPath current;
  int maxcol = 22;

  while (done == 0) {
    int c = fgetc(fi);

    if (c == '\r')
      continue;

    if ((c == '|') || (c == '\n')) {

      if ((c == '\n') && (numcol < 20)) { // Redundant
        string[pos++] = ' ';
        continue;
      }

      string[pos] = '\0';

      //  Assign to the current path structure...
      switch (numcol) {
      case 1:
        current.systemId = atoll(string);
        break;
      case 4:
        strncpy(current.callsign, string, 11);
        break;
      case 6:
        current.pathNumber = atoi(string);
        break;
      case 7:
        current.txLocationNumber = atoi(string);
        break;
      case 8:
        current.txAntennaNumber = atoi(string);
        break;
      case 9:
        current.rxLocationNumber = atoi(string);
        break;
      case 10:
        current.rxAntennaNumber = atoi(string);
        break;
      case 12:
        strncpy(current.pathType, string, 21);
        break;
      case 13:
        current.passiveReceiver = string[0];
        break;
      case 14:
        strncpy(current.countryCode, string, 4);
        break;
      case 15:
        current.GSOinterference = string[0];
        break;
      case 16:
        strncpy(current.rxCallsign, string, 11);
        break;
      case 17:
        current.angularSeparation = emptyAtof(string);
        break;
      case 20:
        current.statusCode = string[0];
        break;
      case 21:
        strncpy(current.statusDate, string, 11);
        break;
      }

      pos = 0;
      numcol++;
      if (numcol == maxcol) { // Done!
        bool replace = false;
        if (strlen(current.rxCallsign) == 0)
          replace = true;
        else {
          char buf[11];
          unsigned int i;

          for (i = 0; i < strlen(current.rxCallsign); i++) {
            buf[i] = toupper(current.rxCallsign[i]);
          }
          buf[i] = '\0';

          if (strstr(current.rxCallsign, "NEW") != NULL)
            replace = true;
          else {
            bool empty = true;
            for (i = 0; i < strlen(buf); i++) {
              if (isspace(buf[i]) == 0)
                empty = false;
            }
            if (empty == true)
              replace = true;
          }
        }
        if (replace == true) {
          strcpy(current.rxCallsign, current.callsign);
        }
        SetToNextLine(fi);
        break;
      }
    } else {
      if (c == EOF)
        break;

      if (c == '\n') {
        c = ' ';

        if (numcol == 0)
          continue;
      }

      string[pos++] = char(c);
    }
  }

  allPaths << current;
  pathMap[current.callsign] << current;
  return;
}

void UlsFileReader::readIndividualEmission(FILE *fi) {
  char string[1024];
  int pos;
  int numcol;
  int maxcol = 16, c;
  char done;

  //  Read in the string.
  done = 0;
  pos = 0;
  numcol = 0;
  UlsEmission current;
  while (done == 0) {
    c = fgetc(fi);

    if (c == '\r')
      continue;

    if ((c == '|') || (c == '\n')) {

      if ((c == '\n') && (numcol < 15)) { // Redundant
        string[pos++] = ' ';
        continue;
      }

      string[pos] = '\0';

      //  Assign to the current path structure...
      switch (numcol) {
      case 1:
        current.systemId = atoll(string);
        break;
      case 4:
        strncpy(current.callsign, string, 11);
        break;
      case 5:
        current.locationId = atoi(string);
        break;
      case 6:
        current.antennaId = atoi(string);
        break;
      case 7:
        current.frequency = emptyAtof(string);
        break;
      case 9:
        strncpy(current.desig, string, 11);
        break;
      case 10:
        current.modRate = emptyAtof(string);
        break;
      case 11:
        strncpy(current.modCode, string, 256);
        break;
      case 12:
        current.frequencyId = atoi(string);
        break;
      }

      pos = 0;
      numcol++;
      if (numcol == maxcol) { // Done!
        SetToNextLine(fi);
        break;
      }
    } else {
      if (c == EOF)
        break;

      if (c == '\n') {
        c = ' ';

        if (numcol == 0)
          continue;
      }

      string[pos++] = char(c);
    }
  }
  allEmissions << current;
  emissionMap[current.callsign] << current;
  return;
}

void UlsFileReader::readIndividualMarketFrequency(FILE *fi) {
  char string[1024];
  int pos;
  int numcol;
  int maxcol = 10, c;
  char done;
  //  Read in the string.
  done = 0;
  pos = 0;
  numcol = 0;
  UlsMarketFrequency current;
  while (done == 0) {
    c = fgetc(fi);

    if (c == '\r')
      continue;

    if ((c == '|') || (c == '\n')) {

      if ((c == '\n') && (numcol < maxcol)) { // Redundant
        string[pos++] = ' ';
        continue;
      }

      string[pos] = '\0';

      //  Assign to the current path structure...
      switch (numcol) {
      case 1:
        current.systemId = atoll(string);
        break;
      case 4:
        strncpy(current.callsign, string, 11);
        break;
      case 5:
        strncpy(current.partitionSeq, string, 7);
        break;
      case 6:
        current.lowerFreq = emptyAtof(string);
        break;
      case 7:
        current.upperFreq = emptyAtof(string);
        break;
      }

      pos = 0;
      numcol++;
      if (numcol == maxcol) { // Done!
        SetToNextLine(fi);
        break;
      }
    } else {
      if (c == EOF)
        break;

      if (c == '\n') {
        c = ' ';

        if (numcol == 0)
          continue;
      }

      string[pos++] = char(c);
    }
  }

  allMarketFrequencies << current;
}

void UlsFileReader::readIndividualEntity(FILE *fi) {
  char string[1024];
  int pos;
  int numcol;
  int maxcol = 30, c;
  char done;
  //  Read in the string.
  done = 0;
  pos = 0;
  numcol = 0;
  UlsEntity current;
  while (done == 0) {
    c = fgetc(fi);

    if (c == '\r')
      continue;

    if ((c == '|') || (c == '\n')) {

      if ((c == '\n') && ((numcol + 1) < maxcol)) { // Redundant
        string[pos++] = ' ';
        continue;
      }

      string[pos] = '\0';

      //  Assign to the current path structure...
      switch (numcol) {
      case 1:
        current.systemId = atoll(string);
        break;
      case 4:
        strncpy(current.callsign, string, 11);
        break;
      case 5:
        strncpy(current.entityType, string, 3);
        break;
      case 7:
        strncpy(current.entityName, string, 201);
        break;
      }

      pos = 0;
      numcol++;
      if (numcol == maxcol) { // Done!
        SetToNextLine(fi);
        break;
      }
    } else {
      if (c == EOF)
        break;

      if (c == '\n') {
        c = ' ';

        if (numcol == 0)
          continue;
      }

      string[pos++] = char(c);
    }
  }

  allEntities << current;
  entityMap[current.callsign] << current;
}

void UlsFileReader::readIndividualLocation(FILE *fi) {
  char string[1024];
  int pos;
  int numcol;
  int maxcol = 51, c;
  char done;
  //  Read in the string.
  done = 0;
  pos = 0;
  numcol = 0;
  UlsLocation current;
  while (done == 0) {
    c = fgetc(fi);

    if (c == '\r')
      continue;

    if ((c == '|') || (c == '\n')) {

      if ((c == '\n') && ((numcol + 1) < maxcol)) { // Redundant
        string[pos++] = ' ';
        continue;
      }

      string[pos] = '\0';

      //  Assign to the current path structure...
      switch (numcol) {
      case 1:
        current.systemId = atoll(string);
        break;
      case 4:
        strncpy(current.callsign, string, 11);
        break;

      case 5:
        current.locationAction = string[0];
        break;
      case 6:
        current.locationType = string[0];
        break;
      case 7:
        current.locationClass = string[0];
        break;
      case 8:
        current.locationNumber = atoi(string);
        break;
      case 9:
        current.siteStatus = string[0];
        break;
      case 10:
        current.correspondingFixedLocation = atoi(string);
        break;
      case 11:
        strncpy(current.locationAddress, string, 81);
        break;
      case 12:
        strncpy(current.locationCity, string, 21);
        break;
      case 13:
        strncpy(current.locationCounty, string, 61);
        break;
      case 14:
        strncpy(current.locationState, string, 3);
        break;
      case 15:
        current.radius = emptyAtof(string);
        break;
      case 16:
        current.areaOperationCode = string[0];
        break;
      case 17:
        current.clearanceIndication = string[0];
        break;
      case 18:
        current.groundElevation = emptyAtof(string);
        break;
      case 19:
        current.latitude = atoi(string);
        current.latitudeDeg = atoi(string);
        break;
      case 20:
        current.latitude = current.latitude + atoi(string) / 60.0;
        current.latitudeMinutes = atoi(string);
        break;
      case 21:
        current.latitude = current.latitude + emptyAtof(string) / 3600.0;
        current.latitudeSeconds = emptyAtof(string);
        break;
      case 22:
        if (string[0] == 'S')
          current.latitude = current.latitude * -1;
        current.latitudeDirection = string[0];
        break;
      case 23:
        current.longitude = atoi(string);
        current.longitudeDeg = atoi(string);
        break;
      case 24:
        current.longitude = current.longitude + atoi(string) / 60.0;
        current.longitudeMinutes = atoi(string);
        break;
      case 25:
        current.longitude = current.longitude + emptyAtof(string) / 3600.0;
        current.longitudeSeconds = emptyAtof(string);
        break;
      case 26:
        if (string[0] == 'W')
          current.longitude = current.longitude * -1;
        current.longitudeDirection = string[0];
        break;
      case 35:
        current.nepa = string[0];
        break;
      case 38:
        current.supportHeight = emptyAtof(string);
        break;
      case 39:
        current.overallHeight = emptyAtof(string);
        break;
      case 40:
        strncpy(current.structureType, string, 7);
        break;
      case 41:
        strncpy(current.airportId, string, 5);
        break;
      case 42:
        strncpy(current.locationName, string, 21);
        break;
      case 48:
        current.statusCode = string[0];
        break;
      case 49:
        strncpy(current.statusDate, string, 11);
        break;
      case 50:
        current.earthStationAgreement = string[0];
        break;
      }
      // qDebug() << "numcol =" << numcol << "vs" << maxcol << "str" << string;

      pos = 0;
      numcol++;
      if (numcol == maxcol) { // Done!
        SetToNextLine(fi);
        break;
      }
    } else {
      if (c == EOF)
        break;

      if (c == '\n') {
        c = ' ';

        if (numcol == 0)
          continue;
      }

      string[pos++] = char(c);
    }
  }

  allLocations << current;
  locationMap[current.callsign] << current;
}

void UlsFileReader::readIndividualAntenna(FILE *fi) {
  char string[1024];
  int pos;
  int numcol;
  int maxcol = 38, c;
  char done;

  //  Read in the string.
  done = 0;
  pos = 0;
  numcol = 0;
  UlsAntenna current;

  while (done == 0) {
    c = fgetc(fi);

    if (c == '\r')
      continue;

    if ((c == '|') || (c == '\n')) {

      if ((c == '\n') && ((numcol + 1) < maxcol)) { // Redundant
        string[pos++] = ' ';
        continue;
      }

      string[pos] = '\0';

      //  Assign to the current path structure...
      switch (numcol) {
      case 1:
        current.systemId = atoll(string);
        break;
      case 4:
        strncpy(current.callsign, string, 11);
        break;

      case 6:
        current.antennaNumber = atoi(string);
        break;
      case 7:
        current.locationNumber = atoi(string);
        break;
      case 8:
        strncpy(current.recvZoneCode, string, 6);
        break;
      case 9:
        current.antennaType = string[0];
        break;
      case 10:
        current.heightToTip = emptyAtof(string);
        break;
      case 11:
        current.heightToCenterRAAT = emptyAtof(string);
        break;
      case 12:
        strncpy(current.antennaMake, string, 26);
        break;
      case 13:
        strncpy(current.antennaModel, string, 26);
        break;
      case 14:
        current.tilt = emptyAtof(string);
        break;
      case 15:
        strncpy(current.polarizationCode, string, 5);
        break;
      case 16:
        current.beamwidth = emptyAtof(string);
        break;
      case 17:
        current.gain = emptyAtof(string);
        break;
      case 18:
        current.azimuth = emptyAtof(string);
        break;
      case 19:
        current.heightAboveAverageTerrain = emptyAtof(string);
        break;
      case 20:
        current.diversityHeight = emptyAtof(string);
        break;
      case 21:
        current.diversityGain = emptyAtof(string);
        break;
      case 22:
        current.diversityBeam = emptyAtof(string);
        break;
      case 23:
        current.reflectorHeight = emptyAtof(string);
        break;
      case 24:
        current.reflectorWidth = emptyAtof(string);
        break;
      case 25:
        current.reflectorSeparation = emptyAtof(string);
        break;
      case 26:
        current.passiveRepeaterNumber = atoi(string);
        break;
      case 27:
        current.backtobackTxGain = emptyAtof(string);
        break;
      case 28:
        current.backtobackRxGain = emptyAtof(string);
        break;
      case 29:
        strncpy(current.locationName, string, 20);
        break;
      case 30:
        current.passiveRepeaterSequenceId = atoi(string);
        break;
      case 31:
        current.alternativeCGSA = string[0];
        break;
      case 32:
        current.pathNumber = atoi(string);
        break;
      case 33:
        current.lineLoss = emptyAtof(string);
        break;

      case 34:
        current.statusCode = string[0];
        break;
      case 35:
        strncpy(current.statusDate, string, 11);
        break;
      }

      pos = 0;
      numcol++;
      if (numcol == maxcol) { // Done!
        SetToNextLine(fi);
        break;
      }
    } else {
      if (c == EOF)
        break;

      if (c == '\n') {
        c = ' ';

        if (numcol == 0)
          continue;
      }

      string[pos++] = char(c);
    }
  }
  allAntennas << current;
  antennaMap[current.callsign] << current;
  return;
}

void UlsFileReader::readIndividualFrequency(FILE *fi) {
  char string[1024];
  int pos;
  int numcol;
  int maxcol = 30, c;
  char done;
  //  Read in the string.
  done = 0;
  pos = 0;
  numcol = 0;
  UlsFrequency current;

  while (done == 0) {
    c = fgetc(fi);

    if (c == '\r')
      continue;

    if ((c == '|') || (c == '\n')) {

      if ((c == '\n') && (numcol < 27)) { // Redundant
        string[pos++] = ' ';
        continue;
      }

      string[pos] = '\0';

      //  Assign to the current path structure...
      switch (numcol) {
      case 1:
        current.systemId = atoll(string);
        break;
      case 4:
        strncpy(current.callsign, string, 11);
        break;

      case 6:
        current.locationNumber = atoi(string);
        break;
      case 7:
        current.antennaNumber = atoi(string);
        break;

      case 8:
        strncpy(current.classStationCode, string, 5);
        break;
      case 9:
        strncpy(current.opAltitudeCode, string, 3);
        break;
      case 10:
        current.frequencyAssigned = emptyAtof(string);
        break;
      case 11:
        current.frequencyUpperBand = emptyAtof(string);
        break;
      case 12:
        current.frequencyCarrier = emptyAtof(string);
        break;
      case 13:
        current.timeBeginOperations = atoi(string);
        break;
      case 14:
        current.timeEndOperations = atoi(string);
        break;
      case 15:
        current.powerOutput = emptyAtof(string);
        break;
      case 16:
        current.powerERP = emptyAtof(string);
        break;
      case 17:
        current.tolerance = emptyAtof(string);
        break;
      case 18:
        current.frequencyIndicator = string[0];
        break;
      case 19:
        current.status = string[0];
        break;
      case 20:
        current.EIRP = emptyAtof(string);
        break;
      case 21:
        strncpy(current.transmitterMake, string, 26);
        break;
      case 22:
        strncpy(current.transmitterModel, string, 26);
        break;
      case 23:
        current.transmitterPowerControl = string[0];
        break;
      case 24:
        current.numberUnits = atoi(string);
        break;
      case 25:
        current.numberReceivers = atoi(string);
        break;
      case 26:
        current.frequencyNumber = atoi(string);
        break;

      case 27:
        current.statusCode = string[0];
        break;
      case 28:
        strncpy(current.statusDate, string, 11);
        break;
      }

      pos = 0;
      numcol++;
      // qDebug() << numcol << maxcol << string;
      if (numcol == maxcol) { // Done!
        SetToNextLine(fi);
        break;
      }
    } else {
      if (c == EOF)
        break;

      if (c == '\n') {
        c = ' ';

        if (numcol == 0)
          continue;
      }

      string[pos++] = char(c);
    }
  }

  //  allFrequencies should now contain all the antenna records in the original
  //  DB.
  allFrequencies << current;
}

void UlsFileReader::readIndividualHeader(FILE *fi) {
  char string[1024];
  int pos;
  int numcol;
  int maxcol = 59, c;
  char done;
  //  Read in the string.
  done = 0;
  pos = 0;
  numcol = 0;
  UlsHeader current;
  while (done == 0) {
    c = fgetc(fi);

    if (c == '\r')
      continue;

    if ((c == '|') || (c == '\n')) {

      if ((c == '\n') && ((numcol + 1) < maxcol)) { // Redundant
        string[pos++] = ' ';
        continue;
      }

      string[pos] = '\0';

      //  Assign to the current structure...
      switch (numcol) {
      case 1:
        current.systemId = atoll(string);
        break;
      case 4:
        strncpy(current.callsign, string, 11);
        break;
      case 5:
        current.licenseStatus = string[0];
        break;
      case 6:
        strncpy(current.radioServiceCode, string, 3);
        break;
      case 7:
        strncpy(current.grantDate, string, 14);
        break;
      case 8:
        strncpy(current.expiredDate, string, 14);
        break;
      case 21:
        current.commonCarrier = string[0];
        break;
      case 22:
        current.nonCommonCarrier = string[0];
        break;
      case 23:
        current.privateCarrier = string[0];
        break;
      case 24:
        current.fixed = string[0];
        break;
      case 25:
        current.mobile = string[0];
        break;
      case 26:
        current.radiolocation = string[0];
        break;
      case 27:
        current.satellite = string[0];
        break;
      case 28:
        current.developmental = string[0];
        break;
      case 29:
        current.interconnected = string[0];
        break;
      case 42:
        strncpy(current.effectiveDate, string, 14);
        break;
      }

      pos = 0;
      numcol++;
      if (numcol == maxcol) { // Done!
        SetToNextLine(fi);
        break;
      }
    } else {
      if (c == EOF)
        break;

      if (c == '\n') {
        c = ' ';

        if (numcol == 0)
          continue;
      }

      string[pos++] = char(c);
    }
  }
  
  //  allHeaders should now contain all the header records in the original DB.
  allHeaders << current;
  headerMap[current.callsign] << current;
}

void UlsFileReader::readIndividualControlPoint(FILE *fi) {
  char string[1024];

  //  Read in the string.
  char done = 0;
  int pos = 0;
  int numcol = 0;
  UlsControlPoint current;
  int maxcol = 14;

  while (done == 0) {
    int c = fgetc(fi);

    if (c == '\r')
      continue;

    if ((c == '|') || (c == '\n')) {
      if ((c == '\n') && ((numcol + 1) < maxcol)) { // Redundant
        string[pos++] = ' ';
        continue;
      }

      string[pos] = '\0';

      //  Assign to the current path structure...
      switch (numcol) {
      case 1:
        current.systemId = atoll(string);
        break;
      case 4:
        strncpy(current.callsign, string, 11);
        break;
      case 5:
        current.controlPointActionPerformed = string[0];
        break;
      case 6:
        current.controlPointNumber = atoi(string);
        break;
      case 7:
        strncpy(current.controlPointAddress, string, 81);
        break;
      case 8:
        strncpy(current.controlPointCity, string, 21);
        break;
      case 9:
        strncpy(current.controlPointState, string, 3);
        break;
      case 10:
        strncpy(current.controlPointPhone, string, 11);
        break;
      case 11:
        strncpy(current.controlPointCounty, string, 61);
        break;
      case 12:
        strncpy(current.controlPointStatus, string, 1);
        break;
      case 13:
        strncpy(current.controlPointStatusDate, string, 14);
        break;
      }

      pos = 0;
      numcol++;
      if (numcol == maxcol) { // Done!
        SetToNextLine(fi);
        break;
      }
    } else {
      if (c == EOF)
        break;

      if (c == '\n') {
        c = ' ';

        if (numcol == 0)
          continue;
      }

      string[pos++] = char(c);
    }
  }

  allControlPoints << current;
  controlPointMap[current.callsign] << current;
}

void UlsFileReader::readIndividualSegment(FILE *fi) {
  char string[1024];

  //  Read in the string.
  char done = 0;
  int pos = 0;
  int numcol = 0;
  UlsSegment current;
  int maxcol = 15;

  while (done == 0) {
    int c = fgetc(fi);

    if (c == '\r')
      continue;

    if ((c == '|') || (c == '\n')) {
      if ((c == '\n') && ((numcol + 1) < maxcol)) { // Redundant
        string[pos++] = ' ';
        continue;
      }

      string[pos] = '\0';

      //  Assign to the current path structure...
      switch (numcol) {
      case 1:
        current.systemId = atoll(string);
        break;
      case 4:
        strncpy(current.callsign, string, 11);
        break;
      case 6:
        current.pathNumber = atoi(string);
        break;
      case 7:
        current.txLocationId = atoi(string);
        break;
      case 8:
        current.txAntennaId = atoi(string);
        break;
      case 9:
        current.rxLocationId = atoi(string);
        break;
      case 10:
        current.rxAntennaId = atoi(string);
        break;
      case 11:
        current.segmentNumber = atoi(string);
        break;
      case 12:
        current.segmentLength = emptyAtof(string);
        break;
      }

      pos = 0;
      numcol++;
      if (numcol == maxcol) { // Done!
        SetToNextLine(fi);
        break;
      }
    } else {
      if (c == EOF)
        break;

      if (c == '\n') {
        c = ' ';

        if (numcol == 0)
          continue;
      }

      string[pos++] = char(c);
    }
  }

  allSegments << current;
  segmentMap[current.callsign] << current;
}