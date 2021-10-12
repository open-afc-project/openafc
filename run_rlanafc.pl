#!/usr/bin/perl -w
################################################################################
#### FILE: run_rlanafc.pl                                                   ####
#### Script to automate running 6GHz RLAN AFC interference simulation       ####
#### Michael Mandell - 2017.06.26                                           ####
#### Minor edits for FB AFC tool by Andrew Winter - 2019.07.19              ####
################################################################################

use File::Path;
use POSIX;
use strict;

my $pi = 4*atan2(1.0,1.0);
my $earthRadius = 6378.137e3;
my $interactive = 0;
my $query       = 1;
my $sim_dir_pfx = "sim/";
my $run_sim_flag = 0;

my $hostname = `hostname`;
chomp $hostname;

print "Running on $hostname\n";

my $caseIdx = 0;

my $outdoorProbHtIdx = 1;
my $outdoorProbHt;

if ($outdoorProbHtIdx == 0) {
    # Outdoor height Parameters 2017.08.23
    $outdoorProbHt = "95:1.5,2:4.5,2:7.5,1:28.5";
} elsif ($outdoorProbHtIdx == 1) {
    # Updated outdoor height Parameters 2017.10.11
    $outdoorProbHt = "95:1.5,2:4.5,2:7.5,0.5:10.5,0.5:28.5";
} else {
    die "ERROR: $!\n";
}

my $indoorProbHtIdx = 1;
my $indoorUrbanCorporateProbHt;
my $indoorUrbanPublicProbHt;
my $indoorUrbanHomeProbHt;
my $indoorSuburbanHomeProbHt;
my $indoorRuralProbHt;

if ($indoorProbHtIdx == 0) {
    # Indoor height Parameters 2017.08.13
    $indoorUrbanCorporateProbHt =    "69.00:1.5"
                             . "," . "21.00:4.5"
                             . "," .  "7.00:7.5"
                             . "," .  "0.70:10.5"
                             . "," .  "0.58:13.5"
                             . "," .  "0.50:16.5"
                             . "," .  "0.43:19.5"
                             . "," .  "0.35:22.5"
                             . "," .  "0.28:25.5"
                             . "," .  "0.20:28.5";

    $indoorUrbanPublicProbHt =       "69.00:1.5"
                             . "," . "21.00:4.5"
                             . "," .  "7.00:7.5"
                             . "," .  "0.70:10.5"
                             . "," .  "0.60:13.5"
                             . "," .  "0.50:16.5"
                             . "," .  "0.40:19.5"
                             . "," .  "0.40:22.5"
                             . "," .  "0.30:25.5"
                             . "," .  "0.20:28.5";

    $indoorUrbanHomeProbHt =         "60.00:1.5"
                             . "," . "30.00:4.5"
                             . "," .  "7.00:7.5"
                             . "," .  "0.70:10.5"
                             . "," .  "0.60:13.5"
                             . "," .  "0.50:16.5"
                             . "," .  "0.40:19.5"
                             . "," .  "0.40:22.5"
                             . "," .  "0.30:25.5"
                             . "," .  "0.20:28.5";

    $indoorSuburbanHomeProbHt =      "60.00:1.5"
                             . "," . "30.00:4.5"
                             . "," .  "5.00:7.5"
                             . "," .  "5.00:10.5";

    $indoorRuralProbHt =             "70.00:1.5"
                             . "," . "25.00:4.5"
                             . "," .  "5.00:7.5";
} elsif ($indoorProbHtIdx == 1) {
    # Indoor height Parameters 2017.11.16
    $indoorUrbanCorporateProbHt =    "82.35:1.5"
                             . "," . "13.35:4.5"
                             . "," .  "2.85:7.5"
                             . "," .  "0.52:10.5"
                             . "," .  "0.36:13.5"
                             . "," .  "0.24:16.5"
                             . "," .  "0.16:19.5"
                             . "," .  "0.09:22.5"
                             . "," .  "0.05:25.5"
                             . "," .  "0.02:28.5";

    $indoorUrbanPublicProbHt = $indoorUrbanCorporateProbHt;

    $indoorUrbanHomeProbHt =         "77.85:1.5"
                             . "," . "17.85:4.5"
                             . "," .  "2.85:7.5"
                             . "," .  "0.52:10.5"
                             . "," .  "0.36:13.5"
                             . "," .  "0.24:16.5"
                             . "," .  "0.16:19.5"
                             . "," .  "0.09:22.5"
                             . "," .  "0.05:25.5"
                             . "," .  "0.02:28.5";

    $indoorSuburbanHomeProbHt =      "77.92:1.5"
                             . "," . "17.92:4.5"
                             . "," .  "2.92:7.5"
                             . "," .  "1.25:10.5";

    $indoorRuralProbHt =             "84.17:1.5"
                             . "," . "14.17:4.5"
                             . "," .  "1.67:7.5";
} else {
    die "ERROR: $!\n";
}

my $fullLidarDataFileList;

$fullLidarDataFileList =
                           "/mnt/usb/lidar_alionnew/Akron_OH"
                   . "," . "/mnt/usb/lidar_alionnew/Albany_NY"
                   . "," . "/mnt/usb/lidar_alionnew/Albuquerque_NM"
#                   . "," . "/mnt/usb/lidar_alionnew/Allentown_PA"
                   . "," . "/mnt/usb/lidar_alionnew/Atlanta_GA"
                   . "," . "/mnt/usb/lidar_alionnew/Austin_TX"
                   . "," . "/mnt/usb/lidar_alionnew/Bakersfield_CA"
                   . "," . "/mnt/usb/lidar_alionnew/Baltimore_MD"
                   . "," . "/mnt/usb/lidar_alionnew/Baton_Rouge_LA"
#                   . "," . "/mnt/usb/lidar_alionnew/Birmingham_AL"
                   . "," . "/mnt/usb/lidar_alionnew/Boston_MA"
                   . "," . "/mnt/usb/lidar_alionnew/Bridgeport_CT"
                   . "," . "/mnt/usb/lidar_alionnew/Buffalo_NY"
                   . "," . "/mnt/usb/lidar_alionnew/Charleston_SC"
                   . "," . "/mnt/usb/lidar_alionnew/Charlotte_NC"
                   . "," . "/mnt/usb/lidar_alionnew/Chicago_IL"
                   . "," . "/mnt/usb/lidar_alionnew/Cincinnati_OH"
                   . "," . "/mnt/usb/lidar_alionnew/Cleveland_OH"
                   . "," . "/mnt/usb/lidar_alionnew/Columbus_OH"
                   . "," . "/mnt/usb/lidar_alionnew/Dallas_TX"
                   . "," . "/mnt/usb/lidar_alionnew/Dayton_OH"
                   . "," . "/mnt/usb/lidar_alionnew/Denver_CO"
                   . "," . "/mnt/usb/lidar_alionnew/Detroit_MI"
                   . "," . "/mnt/usb/lidar_alionnew/El_Paso_TX"
                   . "," . "/mnt/usb/lidar_alionnew/Fort_Wayne_IN"
                   . "," . "/mnt/usb/lidar_alionnew/Fresno_CA"
                   . "," . "/mnt/usb/lidar_alionnew/Grand_Rapids_MI"
                   . "," . "/mnt/usb/lidar_alionnew/Greensboro_NC"
                   . "," . "/mnt/usb/lidar_alionnew/Harrisburg_PA"
                   . "," . "/mnt/usb/lidar_alionnew/Honolulu_HI"
                   . "," . "/mnt/usb/lidar_alionnew/Houston_TX"
                   . "," . "/mnt/usb/lidar_alionnew/Indianapolis_IN"
                   . "," . "/mnt/usb/lidar_alionnew/Kansas_City_MO"
                   . "," . "/mnt/usb/lidar_alionnew/Knoxville_TN"
                   . "," . "/mnt/usb/lidar_alionnew/Lansing_MI"
                   . "," . "/mnt/usb/lidar_alionnew/Las_Vegas_NV"
                   . "," . "/mnt/usb/lidar_alionnew/Little_Rock_AR"
                   . "," . "/mnt/usb/lidar_alionnew/Louisville_KY"
                   . "," . "/mnt/usb/lidar_alionnew/Memphis_TN"
                   . "," . "/mnt/usb/lidar_alionnew/Miami_FL"
                   . "," . "/mnt/usb/lidar_alionnew/Milwaukee_WI"
                   . "," . "/mnt/usb/lidar_alionnew/Minneapolis_MN"
                   . "," . "/mnt/usb/lidar_alionnew/Mobile_AL"
                   . "," . "/mnt/usb/lidar_alionnew/Nashville_TN"
#                   . "," . "/mnt/usb/lidar_alionnew/New_Bedford_MA"
#                   . "," . "/mnt/usb/lidar_alionnew/New_Brunswich_NJ"
                   . "," . "/mnt/usb/lidar_alionnew/New_Haven_CT"
                   . "," . "/mnt/usb/lidar_alionnew/New_Orleans_LA"
                   . "," . "/mnt/usb/lidar_alionnew/New_York_NY"
                   . "," . "/mnt/usb/lidar_alionnew/Norfolk_VA"
#                   . "," . "/mnt/usb/lidar_alionnew/Northeast_PA"
                   . "," . "/mnt/usb/lidar_alionnew/Oklahoma_City_OK"
                   . "," . "/mnt/usb/lidar_alionnew/Omaha_NE"
                   . "," . "/mnt/usb/lidar_alionnew/Orlando_FL"
                   . "," . "/mnt/usb/lidar_alionnew/Oxnard-Ventura_CA"
#                   . "," . "/mnt/usb/lidar_alionnew/Palm_Beach_FL"
                   . "," . "/mnt/usb/lidar_alionnew/Philadelphia_PA"
                   . "," . "/mnt/usb/lidar_alionnew/Phoenix_AZ"
                   . "," . "/mnt/usb/lidar_alionnew/Pittsburgh_PA"
                   . "," . "/mnt/usb/lidar_alionnew/Portland_OR"
                   . "," . "/mnt/usb/lidar_alionnew/Providence_RI"
                   . "," . "/mnt/usb/lidar_alionnew/Raleigh_NC"
                   . "," . "/mnt/usb/lidar_alionnew/Richmond_VA"
                   . "," . "/mnt/usb/lidar_alionnew/Rochester_NY"
                   . "," . "/mnt/usb/lidar_alionnew/Sacramento_CA"
#                   . "," . "/mnt/usb/lidar_alionnew/Saginaw_MI"
                   . "," . "/mnt/usb/lidar_alionnew/Salt_Lake_City_UT"
                   . "," . "/mnt/usb/lidar_alionnew/San_Antonio_TX"
                   . "," . "/mnt/usb/lidar_alionnew/San_Diego_CA"
                   . "," . "/mnt/usb/lidar_alionnew/San_Francisco_CA"
#                   . "," . "/mnt/usb/lidar_alionnew/San_Jose_CA"
                   . "," . "/mnt/usb/lidar_alionnew/San_Juan_PR"
                   . "," . "/mnt/usb/lidar_alionnew/Seattle_WA"
#                   . "," . "/mnt/usb/lidar_alionnew/Shreveport_LA"
                   . "," . "/mnt/usb/lidar_alionnew/Springfield_MA"
                   . "," . "/mnt/usb/lidar_alionnew/St_Louis_MO"
                   . "," . "/mnt/usb/lidar_alionnew/Syracuse_NY"
                   . "," . "/mnt/usb/lidar_alionnew/Toledo_OH"
                   . "," . "/mnt/usb/lidar_alionnew/Tucson_AZ"
                   . "," . "/mnt/usb/lidar_alionnew/Tulsa_OK"
#                   . "," . "/mnt/usb/lidar_alionnew/West_Palm_Beach_FL"
                   . "," . "/mnt/usb/lidar_alionnew/Wichita_KS"
                   . "," . "/mnt/usb/lidar_alionnew/Wilmington_DE"
#                   . "," . "/mnt/usb/lidar_alionnew/Wolf_Mountain_MW_Rx"
#                   . "," . "/mnt/usb/lidar_alionnew/Worcester_MA"
#                   . "," . "/mnt/usb/lidar_alionnew/Yakima_Firing_Range"
#                   . "," . "/mnt/usb/lidar_alionnew/York_PA"

                   . "," . "/mnt/usb/directlidar/Tampa_FL"
                   . "," . "/mnt/usb/directlidar/Washington_DC"

                   . "," . "/mnt/usb/lidar_181219/Cape_Coral_FL_2009_MSL"
                   . "," . "/mnt/usb/lidar_181219/Charleston_WV_2009_MSL"
                   . "," . "/mnt/usb/lidar_181219/Englewood_FL_2009_MSL"
                   . "," . "/mnt/usb/lidar_181219/Jacksonville_FL_2004_MSL"
                   . "," . "/mnt/usb/lidar_181219/Los_Angeles_2007_MSL";
#                   . "," . "/mnt/usb/lidar_181219/Philadelphia_2007_MSL";


my $caseList = {};

for(my $runIdx = 0; $runIdx<316; $runIdx++) {
    my $region = "conus";
    my $activeDeviceFlag = "CEPT_MID";

    my $numRLANUrbanCorporate;
    my $numRLANUrbanPublic;
    my $numRLANUrbanHome;
    my $numRLANSuburbanCorporate;
    my $numRLANSuburbanPublic;
    my $numRLANSuburbanHome;
    my $numRLANRuralCorporate;
    my $numRLANRuralPublic;
    my $numRLANRuralHome;
    my $numRLANBarrenCorporate;
    my $numRLANBarrenPublic;
    my $numRLANBarrenHome;

    my $wlanMinFreq = 5935.0e6;
    my $wlanMaxFreq = 7125.0e6;
    my $spectrumMapMinFreq = 5935.0e6;
    my $spectrumMapMaxFreq = 7125.0e6;

    my $exclusionDist = 1.0;
    my $maxRadius = 150.0e3;
    my $oobRadius = 50.0e3;
    my $pathLossModel = "COALITION_OPT_6";
    # my $pathLossModel = "P.1411_ITM";
    my $clutterFile = "NONE";
    my $fixedProbFlag = "false";
    my $nearFieldLossFlag = "false";
    my $illuminationEfficiency = 1.0;
    my $noBuildingPenetrationLossFlag = "false";
    my $noPolarizationLossFlag = "false";
    my $removeMobile = 1;
    my $interferenceMitigation = 0;
    my $interferenceMitigationThreshold = -6;
    my $visibilityThreshold = -20.0;
    my $fresnelThreshold = 1.0;
    my $closeInDist = 1000;
    my $lidarBeamAttnDB = 10.0;
    my $lidarDataFileList = $fullLidarDataFileList;
    my $maxLidarRegionLoad = ( ($hostname eq "wellington") ? 5 :
                               ($hostname eq "yarmouth"  ) ? 25 :  15 );
    my $lidarWorkingDir  = ( ($hostname eq "wellington") ? "/mnt/usb/lidar_working_dir" :
                             ($hostname eq "yarmouth"  ) ? "/data/tmp/rkf_data/lidar_data/lidar_working_dir" : "/mnt/lidar");
    my $useLidarZIPFiles = ( ($hostname eq "wellington") ? "true" :
                             ($hostname eq "yarmouth"  ) ? "true" : "false");
    my $lidarNumCvgPoints = 32;
    my $lidarExtractULSFlag = "false";
    my $lidarExtractULSSelFile = "";

    my $lidarTxEIRPDBW = 30.0 - 30.0;
    my $lidarULSBW = 30.0e6;
    my $lidarRLANBW = 94.0e6;
    my $lidarBodyLoss = 0.0;
    my $lidarBldgLossDB = 20.0;
    my $lidarPathClutterLossDB = 0.0;
    my $lidarPolarizationLossDB = 3.0;
    my $lidarCvgConeOnly = "false";
    my $lidarMinOcclusionHt = 10.0;
    my $lidarMinOcclusionDist = 1.0e3;
    my $lidarMaxOcclusionDist = 35.0e3;
    my $lidarTxOcclusionRadius = 55.0;
    my $lidarBlacklistFile = "";

    my $closeInPathLossModel;
    my $closeInHgtFlag;
    my $closeInHgtLOS;
    if (1) {
        $closeInPathLossModel = "WINNER2";
        $closeInHgtFlag = 1;
        $closeInHgtLOS = 15.0;
    } else {
        $closeInPathLossModel = "M.2135";
        $closeInHgtFlag = 1;
        $closeInHgtLOS = 10.0;
    }
    my $closeInFileFlag = 0;

    my $useEirpPatternFile = 0;
    my $clipRLANOutdoorEIRPFlag = "true";
    my $eirpOption = "SE45"; # "baseline";
    my $ulsPositionOption = "file";
    my $ulsCenterLongitudeDeg = 0.0;
    my $ulsCenterLatitudeDeg  = 0.0;
    my $ulsRadius             = 0.0;
    my $ulsNoiseFigureDB      = 5.0;
    my $numULS                = 0;
    my $adjustSimulationRegion = "true";
    my $useMultiThreading = "true";
    my $rlanSensingBandwidth = 20.0e6;
    my $rlanSensingFrequency = 6580.0e6;
    my $numTrial = 1;
    my $channelPairingFile = "uls_channel_plan.csv";

    my $fsCustomFilterFlag       = "false";
    my $fsCustomFilterLonMin     = 0.0;
    my $fsCustomFilterLonMax     = 0.0;
    my $fsCustomFilterLatMin     = 0.0;
    my $fsCustomFilterLatMax     = 0.0;

    my $fssGonTMethod            = "value";
    my $fssGonTFile              = "";
    my $fssGonTFileOffset        = 0.0;
    my $fssGonT                  = 2.0;
    my $fssGonTMainBeamThr       = -8.0;
    my $fssOrbitalSlotStart      = -1.0;
    my $fssOrbitalSlotIncr       = 2.0;
    my $fssNumOrbitalSlot        = -1;
    my $fssPolarizationMismatch  = 3.0;
    my $fssLinkBudgetN           = 1000;

    my $ISNumRLAN                = -1;
    my $ISAngleCoeffDeg          = 2000.0;
    my $ISDistCoeffKm            = 1000.0;

    my $ISW2UrbLOSShiftMean      = -1.0;
    my $ISW2UrbNLOSShiftMean     = -1.0;
    my $ISW2SubLOSShiftMean      = -1.0;
    my $ISW2SubNLOSShiftMean     = -1.0;
    my $ISP452Eps                = 0.1;

    my $ISP2135UrbLOSShiftMean   = -1.0;
    my $ISP2135UrbNLOSShiftMean  = -1.0;
    my $ISP2135SubLOSShiftMean   = -1.0;
    my $ISP2135SubNLOSShiftMean  = -1.0;
    my $ISP2135RurLOSShiftMean   = -1.0;
    my $ISP2135RurNLOSShiftMean  = -1.0;

    my $terrainMethod            = "SRTM";
    my $terrainMinLon            = -180.0;
    my $terrainMaxLon            =  180.0;
    my $terrainMinLat            = -90.0;
    my $terrainMaxLat            =  90.0;
    my $cityName;
    my $spectrumMapThreshold = -6.0;
    my $spectrumMapLonStart = -1;
    my $spectrumMapLonStop  = -1;
    my $spectrumMapLatStart = -1;
    my $spectrumMapLatStop  = -1;
    my $spectrumMapNumLon   = 2;
    my $spectrumMapNumLat   = 2;
    my $spectrumMapRlanHeight = -1;
    my $spectrumMapRlanEIRP = -500;
    my $spectrumMapBuildingType  = "NO_BUILDING";
    my $scaleNumRLANConus = 1;
    my $outdoorFrac = 0.02; # 2.0 %
    my $bandStr;
    my $ulsFile = "";
    my $name;
    my $analysisMethod;
    my $rlanScaleStr = ":1";
    my $runMitigation = "false";
    my $mtgnRLANTxPwrDBW = -7.0;
    my $heatmapFull = "true";
    my $probIndoorBodyLossRLAN  = "26.32:4,73.68:0"; # 4 dB or 0 dB with respective probabilities
    my $probOutdoorBodyLossRLAN = "50.0:4,50.0:0"; # 4 dB or 0 dB with 50% probability each
    my $filterSimRegionOnly = "false";
    my $compute_pl_20_50_flag = "false";
    my $ulsAntennaPatternFile = "";

    my @cityList;
    my $runType = "singlePoint";

    if ($runIdx == 0) {
        $ulsFile = "output-UL20172211820160-filteredv2.csv";
        # $ulsFile = "ULS_FS_23Jan2019.csv";
        $numTrial = 10;
        $name = "conus_10x";
        $adjustSimulationRegion = "false";
        $analysisMethod = "MONTE_CARLO";
        $ulsNoiseFigureDB      = 1.0;
    } elsif ($runIdx == 1) {
        $ulsFile = "single_urban_uls.csv";
        $numTrial = 1; # 10000;
        $name = "single_urban";
        $analysisMethod = "MONTE_CARLO";
        $nearFieldLossFlag = "true";
        $visibilityThreshold = -10000.0;
    } elsif ($runIdx == 2) {
        $ulsFile = "single_suburban_uls.csv";
        $numTrial = 10000;
        $name = "single_suburban";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 3) {
        $ulsFile = "single_rural_uls.csv";
        $numTrial = 10000;
        $name = "single_rural";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 4) {
        # $ulsFile = "ULS_Data_File_Filtered_Modified_14July2017_T.csv";
        $ulsFile = "ATT_ULS_fixed.csv";
        $numTrial = 100;
        $name = "T";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 5) {
        $ulsFile = "single_urbsubrur_uls.csv";
        $numTrial = 1;
        $name = "single_urbsubrur_dbg";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 6) {
        $ulsFile = "";
        $numTrial = 10000;
        $name = "propagation";
        $analysisMethod = "PROPAGATION";
    } elsif ($runIdx == 7) {
        $ulsFile = "DFW_MSA_ULS.csv";
        $numTrial = 1;
        $name = "DFW";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 8) {
        $ulsFile = "output-UL20172211820160-filteredv2.csv";
        $numTrial = 1;
        $name = "spectrum_map_m6";
        $spectrumMapThreshold = -6.0;
        $analysisMethod = "SPECTRUM_MAP";
    } elsif ($runIdx == 9) {
        $ulsFile = "output-UL20172211820160-filteredv2.csv";
        $numTrial = 1;
        $name = "fs_to_fss";
        $removeMobile = 0;
        $analysisMethod = "ULS_TO_GEO";
    } elsif ($runIdx == 10) {
        $ulsFile = "";
        $numTrial = 1;
        $name = "wifi_to_fss";
        $clipRLANOutdoorEIRPFlag = "false";
        $fssOrbitalSlotIncr  = 0.1;
        $analysisMethod = "RLAN_TO_GEO";
    } elsif ($runIdx == 11) {
        $ulsFile = "conus_m6_uls_test.csv";
        $numTrial = 1000;
        $name = "conus_1000_test";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 12) {
        $ulsFile = "conus_m6_uls.csv";
        $numTrial = 1000;
        $name = "conus_1000";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 13) {
        $ulsFile = "conus_m6_uls.csv";
        $numTrial = 1000;
        $name = "conus_1000_260m";
        $analysisMethod = "MONTE_CARLO";
        $exclusionDist = 260.0;
    } elsif ($runIdx == 14) {
        $ulsFile = "conus_m6_uls.csv";
        $numTrial = 1000;
        $name = "conus_1000_1km";
        $analysisMethod = "MONTE_CARLO";
        $exclusionDist = 1000.0;
    } elsif ($runIdx == 15) {
        $ulsFile = "conus_m6_top10_uls.csv";
        $numTrial = 10000;
        $name = "conus_top10_10K_noexcl";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 16) {
        $ulsFile = "conus_m6_top10_uls.csv";
        $numTrial = 10000;
        $name = "conus_top10_10K_260m";
        $analysisMethod = "MONTE_CARLO";
        $exclusionDist = 260.0;
    } elsif ($runIdx == 17) {
        $ulsFile = "conus_m6_top10_uls.csv";
        $numTrial = 10000;
        $name = "conus_top10_10K_1km";
        $analysisMethod = "MONTE_CARLO";
        $exclusionDist = 1000.0;
    } elsif ($runIdx == 18) {
        $ulsFile = "conus_m6_top10_uls.csv";
        $numTrial = 10000;
        $name = "conus_top10_10K_winner2";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "WINNER2_ITM";
    } elsif ($runIdx == 19) {
        $ulsFile = "conus_top165_uls.csv";
        $numTrial = 1000;
        $name = "conus_top165_1K";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 20) {
        $ulsFile = "mobile_uls.csv";
        $numTrial = 10000;
        $name = "mobile";
        $removeMobile = 0;
        $analysisMethod = "MOBILE_MONTE_CARLO";
    } elsif ($runIdx == 21) {
        $ulsFile = "";
        $numTrial = 10000;
        $name = "dbg_propagation";
        $analysisMethod = "DBG_PROPAGATION";
    } elsif ($runIdx == 22) {
        $ulsFile = "MW-3Cases-ULS-v2_sim.csv";
        $numTrial = 10000;
        $name = "public_safety_mobile";
        $removeMobile = 0;
        $analysisMethod = "MOBILE_MONTE_CARLO";
    } elsif ($runIdx == 23) {
        $ulsFile = "single_uls_intmg.csv";
        $numTrial = 10000;
        $name = "single_uls_intmg_m6";
        $analysisMethod = "MONTE_CARLO";
        $interferenceMitigation = 1;
        $interferenceMitigationThreshold = -6.0;
    } elsif ($runIdx == 24) {
        $ulsFile = "single_uls_intmg.csv";
        $numTrial = 10000;
        $name = "single_uls_intmg_0";
        $analysisMethod = "MONTE_CARLO";
        $interferenceMitigation = 1;
        $interferenceMitigationThreshold = 0.0;
    } elsif ($runIdx == 25) {
        $ulsFile = "single_uls_intmg.csv";
        $numTrial = 10000;
        $name = "single_uls_intmg_6";
        $analysisMethod = "MONTE_CARLO";
        $interferenceMitigation = 1;
        $interferenceMitigationThreshold = 6.0;
    } elsif ($runIdx == 26) {
        $ulsFile = "output-UL20172211820160-filteredv2.csv";
        $numTrial = 1;
        $name = "spectrum_map_0";
        $spectrumMapThreshold = 0.0;
        $analysisMethod = "SPECTRUM_MAP";
    } elsif ($runIdx == 27) {
        $ulsFile = "output-UL20172211820160-filteredv2.csv";
        $numTrial = 1;
        $name = "spectrum_map_6";
        $spectrumMapThreshold = 6.0;
        $analysisMethod = "SPECTRUM_MAP";
    } elsif ($runIdx == 28) {
        # Generate data for spectrum plot of ULS's
        $ulsFile = "output-UL20172211820160-filtered.csv";
        $removeMobile = 0;
        $numTrial = 1;
        $name = "spectrum_plot";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 29) {
        $ulsFile = "single_uls_intmg.csv";
        $numTrial = 10000;
        $name = "w2_sens";
        $analysisMethod = "MONTE_CARLO";
        $closeInHgtFlag = 0;
    } elsif ($runIdx == 30) {
        $ulsFile = "single_uls_intmg.csv";
        $numTrial = 10000;
        $name = "w2_hgt_dep_sens";
        $analysisMethod = "MONTE_CARLO";
        $closeInHgtFlag = 1;
    } elsif ($runIdx == 31) {
        $useEirpPatternFile = 1;
        $ulsFile = "output-UL20172211820160-filteredv2.csv";
        $numTrial = 1;
        $name = "dbg_conus_eirp";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 32) {
        $pathLossModel = "P.1411_ITM";
        $exclusionDist = 0.0;
        $ulsFile = "output-UL20172211820160-filteredv2.csv";
        $numTrial = 1;
        $name = "dbg_conus_pm";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 33) {
        $useEirpPatternFile = 1;
        $pathLossModel = "P.1411_ITM";
        $exclusionDist = 0.0;
        $ulsFile = "single_uls_la.csv";
        $numTrial = 1000;
        $name = "dbg_single_uls_la_origpm_origeirp";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 34) {
        $useEirpPatternFile = 0;
        $pathLossModel = "P.1411_ITM";
        $exclusionDist = 0.0;
        $ulsFile = "single_uls_la.csv";
        $numTrial = 1000;
        $name = "dbg_single_uls_la_origpm_neweirp";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 35) {
        $useEirpPatternFile = 1;
        $pathLossModel = "COALITION_OPT_6";
        $exclusionDist = 30.0;
        $closeInDist = 250.0;
        $ulsFile = "single_uls_la.csv";
        $numTrial = 1000;
        $name = "dbg_single_uls_la_w2pm250_origeirp";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 36) {
        $useEirpPatternFile = 0;
        $pathLossModel = "COALITION_OPT_6";
        $exclusionDist = 30.0;
        $closeInDist = 250.0;
        $ulsFile = "single_uls_la.csv";
        $numTrial = 1000;
        $name = "dbg_single_uls_la_w2pm250_neweirp";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 37) {
        $useEirpPatternFile = 0;
        $pathLossModel = "COALITION_OPT_6";
        $exclusionDist = 30.0;
        $closeInDist = 1000.0;
        $ulsFile = "single_uls_la.csv";
        $numTrial = 1000;
        $name = "dbg_single_uls_la_w2pm1000_neweirp";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 38) {
        $ulsFile = "ULS_23Jan2019_perlink.csv";
        # $ulsFile = "output-UL20172211820160-filteredv2.csv";
        $numTrial = 1;
        $name = "dbg_conus_1x";
        $adjustSimulationRegion = "false";
        $analysisMethod = "MONTE_CARLO";
        $nearFieldLossFlag = "true";
        $visibilityThreshold = -10000.0;
        $useMultiThreading = "false";
        $scaleNumRLANConus = 0.1;
    } elsif ($runIdx == 39) {
        $ulsFile = "output-UL20172211820160-filteredv2.csv";
        $numTrial = 1;
        $closeInDist = 250;
        $name = "dbg_conus_1x_250_ruralwinner2";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 40) {
        $ulsFile = "output-UL20172211820160-filteredv2.csv";
        $numTrial = 1;
        $pathLossModel = "P.1411_ITM_W2PROB";
        $name = "dbg_conus_1x_1411_w2prob";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 41) {
        $pathLossModel = "P.1411_ITM";
        $ulsFile = "output-UL20172211820160-filteredv2.csv";
        $numTrial = 1;
        $name = "dbg_conus_1x_oldpm_neweirp";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 42) {
        $useEirpPatternFile = 1;
        $ulsFile = "output-UL20172211820160-filteredv2.csv";
        $numTrial = 1;
        $closeInDist = 250;
        $name = "dbg_conus_1x_250_oldeirp";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 43) {
        $useEirpPatternFile = 1;
        $ulsFile = "output-UL20172211820160-filteredv2.csv";
        $numTrial = 1;
        $pathLossModel = "P.1411_ITM_W2PROB";
        $name = "dbg_conus_1x_1411_w2prob_oldeirp";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 44) {
        $ulsFile = "output-UL20172211820160-filteredv2.csv";
        $numTrial = 1;
        $pathLossModel = "P.1411_ITM";
        $exclusionDist = 260.0;
        $name = "dbg_conus_1x_oldpm260_neweirp";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 45) {
        # set yearIdx = 3 for original RLAN device densities
        # set probBwAPIdx = 0;
        # set outdoorProbHtIdx = 0;
        $useEirpPatternFile = 1;
        $pathLossModel = "P.1411_ITM";
        $exclusionDist = 30.0;
        $ulsFile = "output-UL20172211820160-filteredv2.csv";
        $numTrial = 1;
        $name = "dbg_conus_1x_oldpm_oldrlan";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 46) {
        $ulsFile = "output-UL20172211820160-filteredv2.csv";
        $numTrial = 10;
        $closeInDist = 250;
        $name = "dbg_conus_10x_250_neweirp";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 47) {
        $useEirpPatternFile = 0;
        $pathLossModel = "P.1411_ITM";
        $exclusionDist = 30.0;
        $ulsFile = "output-UL20172211820160-filteredv2.csv";
        $numTrial = 1;
        $name = "dbg_conus_1x_oldpm_newrlan";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 48) {
        $useEirpPatternFile = 0;
        $closeInDist = 250;
        $exclusionDist = 30.0;
        $ulsFile = "output-UL20172211820160-filteredv2.csv";
        $numTrial = 1;
        $name = "dbg_conus_1x_newpm_newrlan";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 49) {
        $useEirpPatternFile = 1;
        $pathLossModel = "P.1411_ITM";
        $exclusionDist = 30.0;
        $ulsFile = "output-UL20172211820160-filteredv2.csv";
        $numTrial = 1;
        $name = "dbg_conus_1x_oldpm_newrlan_oldeirp";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 50) {
        $useEirpPatternFile = 0;
        $closeInDist = 250;
        $exclusionDist = 30.0;
        $ulsFile = "output-UL20172211820160-filteredv2.csv";
        $numTrial = 1;
        $eirpOption = "indoor_max_reduced_6";
        $name = "dbg_conus_1x_newpm_newrlan_inmax_6";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 51) {
        $useEirpPatternFile = 0;
        $closeInDist = 250;
        $exclusionDist = 30.0;
        $ulsFile = "output-UL20172211820160-filteredv2.csv";
        $numTrial = 1;
        $eirpOption = "indoor_max_reduced_10";
        $name = "dbg_conus_1x_newpm_newrlan_inmax_10";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 52) {
        $useEirpPatternFile = 1;
        $ulsFile = "";
        $numTrial = 1;
        $name = "wifi_to_fss_oldeirp";
        $analysisMethod = "RLAN_TO_GEO";
    } elsif ($runIdx == 53) {
        $ulsFile = "output-UL20172211820160-filteredv2.csv";
        $numTrial = 1;
        $name = "conus_1x";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 54) {
        $ulsFile = "";
        $numTrial = 1;
        $name = "dbg_antenna_1245";
        $analysisMethod = "DBG_ANTENNA_1245";
    } elsif ($runIdx == 55) {
        # $ulsFile = "ULS_Data_File_Filtered_Modified_14July2017_T.csv";
        $ulsFile = "Verizon_ULS_fixed.csv";
        $numTrial = 100;
        $name = "VZ";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 56) {
        # To get count of how many of these 176 are invalid
        $ulsFile = "conus_top176_uls.csv";
        $numTrial = 1000;
        $name = "conus_top176_1K";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 57) {
        $ulsFile = "ATT_excthr20iter_uls.csv";
        $numTrial = 1000;
        $name = "ATT_1000x";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 58) {
        $ulsFile = "VZ_excthr10iter_uls.csv";
        $numTrial = 1000;
        $name = "VZ_1000x";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 59) {
        $ulsFile = "NONE";
        $ulsPositionOption = "radius";
        $ulsCenterLongitudeDeg = -118.214584;
        $ulsCenterLatitudeDeg  =   34.056755;
        $ulsRadius             = 20e3;
        $numULS                = 100;
        $adjustSimulationRegion = "true";
        $numTrial = 100;
        $name = "UWB";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "UWB";
    } elsif ($runIdx == 60) {
        $ulsFile = "Verizon_ULS_fixed.csv";
        $numTrial = 100;
        $maxRadius = 500.0e3;
        $name = "VZ_max_radius_500km";
        $analysisMethod = "MONTE_CARLO";
    } elsif ($runIdx == 61) {
        $ulsFile = "output-UL20172211820160-filteredv2.csv";
        # $ulsFile = "single_uls_pair.csv";
        $name = "dbg_uls_pair";
        $analysisMethod = "DBG_ULS_PAIR";
    } elsif ($runIdx == 62) {
        $ulsFile = "output-UL20172211820160-filteredv2.csv";
        # $ulsFile = "single_uls_pair.csv";
        $name = "rlan_sensing";
        $analysisMethod = "RLAN_SENSING_MONTE_CARLO";
    } elsif ($runIdx == 63) {
        $ulsFile = "ATT_ULS_fixed.csv";
        $numTrial = 1;
        $name = "ATT_showallrlan";
        $analysisMethod = "FS_PWR_AT_RLAN_MONTE_CARLO";
        $visibilityThreshold = -10000.0;
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
    } elsif ($runIdx == 64) {
        $ulsFile = "Verizon_ULS_fixed.csv";
        $numTrial = 1;
        $name = "VZ_showallrlan";
        $analysisMethod = "FS_PWR_AT_RLAN_MONTE_CARLO";
        $visibilityThreshold = -10000.0;
    } elsif ($runIdx == 65) {
        $name = "DBG_BLDG_LOSS";
        $analysisMethod = "DBG_BLDG_LOSS";
        $pathLossModel = "UNKNOWN";
        $ulsFile = "NONE";
        $numTrial = 10000;
    } elsif ($runIdx == 66) {
        $ulsFile = "output-UL20172211820160-filteredv2.csv";
        $numTrial = 10;
        $name = "conus_10x_P452";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
    } elsif ($runIdx == 67) {
        $ulsFile = "output-UL20172211820160-filteredv2.csv";
        $numTrial = 10;
        $name = "conus_10x_P452_EU_RLAN";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
    } elsif ($runIdx == 68) {
        $ulsFile = "FS_Database_Netherlands.csv";
        $numTrial = 10000;
        $name = "NL_10Kx";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
    } elsif ($runIdx == 69) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_ses_4_22w";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $fssGonTMethod       = "gxt";
        $fssGonTFile         = "22W_Hemispheric.gxt";
        $fssGonTFileOffset   = 5.4;
        $fssOrbitalSlotStart = -22.0;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
    } elsif ($runIdx == 70) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_intelsat_33e_60e";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $fssGonTMethod       = "csv";
        $fssGonTFile         = "intelsat_33e_60e_gt.csv";
        $fssGonTFileOffset   = 1.675;
        $fssOrbitalSlotStart = 60.0;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;

        # $analysisMethod = "DBG_CSV";
        # $pathLossModel = "UNKNOWN";
    } elsif ($runIdx == 71) {
        $ulsFile = "FS_Database_UK.csv";
        $numTrial = 1000;
        $name = "UK_1000x";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
    } elsif ($runIdx == 72) {
        $ulsFile = "FS_Database_Netherlands.csv";
        $numTrial = 10000;
        $name = "NL_10Kx_low";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_LOW";
    } elsif ($runIdx == 73) {
        $ulsFile = "FS_Database_Netherlands.csv";
        $numTrial = 10000;
        $name = "NL_10Kx_high";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_HIGH";
    } elsif ($runIdx == 74) {
        $ulsFile = "output-UL20172211820160-filteredv2.csv";
        $numTrial = 1;
        $name = "conus_fs_to_fs";
        $analysisMethod = "FS_TO_FS";
        $pathLossModel = "ITM";
        $eirpOption = "baseline";
        $region = "conus";
        $activeDeviceFlag = "CEPT_HIGH";
    } elsif ($runIdx == 75) {
        $ulsFile = "FS_Database_Netherlands.csv";
        $numTrial = 10000;
        $name = "NL_10Kx_excl_1m";
        $exclusionDist = 1.0;
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
    } elsif ($runIdx == 76) {
        $ulsFile = "FS_Database_UK.csv";
        $numTrial = 1000;
        $name = "UK_1000x_excl_1m";
        $exclusionDist = 1.0;
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
    } elsif ($runIdx == 77) {
        $ulsFile = "FS_Database_UK.csv";
        $numTrial = 8000;
        $name = "UK_8000x_excl_1m";
        $exclusionDist = 1.0;
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
    } elsif ($runIdx <= 78 + 120 - 1) {
        $exclusionDist = 1.0;
        $ulsFile = "output-UL20172211820160-filteredv2.csv";
        $numTrial = 1;
        my $smIdx = $runIdx - 78;
        my $addlAttnIdx = $smIdx % 2;
        $smIdx = ($smIdx - $addlAttnIdx)/2;
        my $bandIdx = $smIdx % 2;
        $smIdx = ($smIdx - $bandIdx)/2;
        my $bldgIdx = $smIdx % 2;
        $smIdx = ($smIdx - $bldgIdx)/2;
        my $eirpIdx = $smIdx % 3;
        $smIdx = ($smIdx - $eirpIdx)/3;
        my $cityIdx = $smIdx % 5;
        if ($cityIdx == 0) {
            $cityName = "losangeles";
            $spectrumMapLonStart = -118.621611;
            $spectrumMapLonStop  = -117.409722;
            $spectrumMapLatStart =   33.638333;
            $spectrumMapLatStop  =   34.819272;
        } elsif ($cityIdx == 1) {
            $cityName = "dallas";
            $spectrumMapLonStart =  -98.082;
            $spectrumMapLonStop  =  -95.865028;
            $spectrumMapLatStart =   31.800111;
            $spectrumMapLatStop  =   33.433278;
        } elsif ($cityIdx == 2) {
            $cityName = "washingtondc";
            $spectrumMapLonStart =  -78.370194;
            $spectrumMapLonStop  =  -76.400250;
            $spectrumMapLatStart =   38.004250;
            $spectrumMapLatStop  =   39.718417;
        } elsif ($cityIdx == 3) {
            $cityName = "sanfrancisco";
            $spectrumMapLonStart = -123.008028;
            $spectrumMapLonStop  = -121.479333;
            $spectrumMapLatStart =   37.116583;
            $spectrumMapLatStop  =   38.310583;
        } elsif ($cityIdx == 4) {
            $cityName = "seattle";
            $spectrumMapLonStart = -122.828219;
            $spectrumMapLonStop  = -120.906739;
            $spectrumMapLatStart =   46.733908;
            $spectrumMapLatStop  =   48.295014;
        } else {
            die "ERROR: $!\n";
        }
        my $maxCos;
        if ($spectrumMapLatStart * $spectrumMapLatStop < 0.0) {
            $maxCos = 1.0;
        } elsif (fabs($spectrumMapLatStart) < fabs($spectrumMapLatStop)) {
            $maxCos = cos($spectrumMapLatStart*$pi/180.0);
        } else {
            $maxCos = cos($spectrumMapLatStop*$pi/180.0);
        }
        my $dlat =         $earthRadius*($spectrumMapLatStop - $spectrumMapLatStart)*$pi/180.0;
        my $dlon = $maxCos*$earthRadius*($spectrumMapLonStop - $spectrumMapLonStart)*$pi/180.0;

        my $gridSpacing    = 1000.0; # Grid spacing in meters
        $spectrumMapNumLon = ceil($dlon / $gridSpacing) + 1;
        $spectrumMapNumLat = ceil($dlat / $gridSpacing) + 1;

        my $deltaLon = ($spectrumMapLonStop-$spectrumMapLonStart)/($spectrumMapNumLon-1);
        my $deltaLat = ($spectrumMapLatStop-$spectrumMapLatStart)/($spectrumMapNumLat-1);

        # print "CITY: $cityName\n";
        # printf "BB_MIN_LON = %10.7f\n", $spectrumMapLonStart - $deltaLon/2;
        # printf "BB_MAX_LON = %10.7f\n", $spectrumMapLonStop  - $deltaLon/2;
        # printf "BB_MIN_LAT = %10.7f\n", $spectrumMapLatStart - $deltaLat/2;
        # printf "BB_MAX_LAT = %10.7f\n", $spectrumMapLatStop  - $deltaLat/2;
        # print "\n";

        if ($eirpIdx == 0) {
            $spectrumMapRlanEIRP = 35 - 30;
        } elsif ($eirpIdx == 1) {
            $spectrumMapRlanEIRP = 24 - 30;
        } elsif ($eirpIdx == 2) {
            $spectrumMapRlanEIRP = 18 - 30;
        } else {
            die "ERROR: $!\n";
        }
        my $spectrumMapBldgStr;
        if ($bldgIdx == 0) {
            $spectrumMapBldgStr = "outdoor";
            $spectrumMapBuildingType = "NO_BUILDING";
            $spectrumMapRlanHeight = 12.0;
        } elsif ($bldgIdx == 1) {
            $spectrumMapBldgStr = "indoor";
            $spectrumMapBuildingType = "TRADITIONAL";
            $spectrumMapRlanHeight = 2.0;
        } else {
            die "ERROR: $!\n";
        }
        if ($bandIdx == 0) {
            $bandStr = "unii5";
            $spectrumMapMinFreq = 5935.0e6;
            $spectrumMapMaxFreq = 6425.0e6;
        } elsif ($bandIdx == 1) {
            $bandStr = "unii78";
            $spectrumMapMinFreq = 6525.0e6;
            $spectrumMapMaxFreq = 7125.0e6;
        } else {
            die "ERROR: $!\n";
        }
        my $addlAttnStr;
        if ($addlAttnIdx == 0) {
            $addlAttnStr = "attn_0";
            $spectrumMapThreshold = -6.0;
        } elsif ($addlAttnIdx == 1) {
            $addlAttnStr = "attn_10";
            $spectrumMapThreshold = 4.0; # Additional 10 dB attenuation
        } else {
            die "ERROR: $!\n";
        }

        my $eirpDBm = $spectrumMapRlanEIRP + 30;
        $name = "spectrum_map_${cityName}_${eirpDBm}_${spectrumMapBldgStr}_${bandStr}_${addlAttnStr}";
        $analysisMethod = "SPECTRUM_MAP";
        $fixedProbFlag = "true";
    } elsif ($runIdx == 198) {
        $ulsFile = "output-UL20172211820160-filteredv2.csv";
        $numTrial = 10;
        $name = "conus_10x_1M_indoor_mean";
        $analysisMethod = "MONTE_CARLO";
        $scaleNumRLANConus = 1000000.0/394956.0;
        $outdoorFrac = 0.0; # all indoor
        $eirpOption = "const_400mw"; # all RLAN EIRP 400 mw => 26 dBm
        $fixedProbFlag = "true";
    } elsif ($runIdx == 199) {
        $ulsFile = "output-UL20172211820160-filteredv2.csv";
        $name = "dbg_terrain_scan";
        $analysisMethod = "DBG_TERRAIN_SCAN";
    } elsif ($runIdx == 200) {
        $ulsFile = "FS_Database_Netherlands.csv";
        $numTrial = 10000;
        $name = "NL_10Kx_mid";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_MID";
    } elsif ($runIdx == 201) {
        $ulsFile = "FS_Database_UK.csv";
        $numTrial = 1000;
        $name = "UK_1000x_mid";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_MID";
    } elsif ($runIdx == 202) {
        $ulsFile = "FS_Database_UK.csv";
        $numTrial = 1000;
        $name = "UK_1000x_high";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_HIGH";
    } elsif ($runIdx == 203) {
        $ulsFile = "";
        $numTrial = 1;
        $name = "wifi_to_fss_with_rstr";
        $clipRLANOutdoorEIRPFlag = "true";
        $fssOrbitalSlotIncr  = 0.1;
        $analysisMethod = "RLAN_TO_GEO";
    } elsif ($runIdx == 204) {
        $ulsFile = "";
        $numTrial = 1;
        $name = "wifi_to_fss_no_rstr";
        $clipRLANOutdoorEIRPFlag = "false";
        $fssOrbitalSlotIncr  = 0.1;
        $analysisMethod = "RLAN_TO_GEO";
    } elsif ($runIdx == 205) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_ses_4_22w_mid";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $fssGonTMethod       = "gxt";
        $fssGonTFile         = "22W_Hemispheric.gxt";
        $fssGonTFileOffset   = 5.4;
        $fssOrbitalSlotStart = -22.0;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
        $activeDeviceFlag = "CEPT_MID";
    } elsif ($runIdx == 206) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_intelsat_33e_60e_mid";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $fssGonTMethod       = "csv";
        $fssGonTFile         = "intelsat_33e_60e_gt.csv";
        $fssGonTFileOffset   = 1.675;
        $fssGonTMainBeamThr = -100.0;
        $fssOrbitalSlotStart = 60.0;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
        $activeDeviceFlag = "CEPT_MID";
    } elsif ($runIdx == 207) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_is_35e_mid";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $fssGonTMethod       = "gxt";
        $fssGonTFile         = "IS-35e_C01.gxt";
        $fssGonTFileOffset   = 16.39;
        $fssOrbitalSlotStart = -34.5;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
        $activeDeviceFlag = "CEPT_MID";
    } elsif ($runIdx == 208) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_is_14_mid";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $fssGonTMethod       = "gxt";
        $fssGonTFile         = "IS-14_CEUH.gxt";
        $fssGonTFileOffset   = 6.29;
        $fssOrbitalSlotStart = -45.0;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
        $activeDeviceFlag = "CEPT_MID";
    } elsif ($runIdx == 209) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_is_22_mid";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $fssGonTMethod       = "gxt";
        $fssGonTFile         = "IS-22_WHLU.gxt";
        $fssGonTFileOffset   = 4.30;
        $fssOrbitalSlotStart = 72.0;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
        $activeDeviceFlag = "CEPT_MID";
    } elsif ($runIdx == 210) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_ses_4_22w_high";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $fssGonTMethod       = "gxt";
        $fssGonTFile         = "22W_Hemispheric.gxt";
        $fssGonTFileOffset   = 5.4;
        $fssOrbitalSlotStart = -22.0;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
        $activeDeviceFlag = "CEPT_HIGH";
    } elsif ($runIdx == 211) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_intelsat_33e_60e_high";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $fssGonTMethod       = "csv";
        $fssGonTFile         = "intelsat_33e_60e_gt.csv";
        $fssGonTFileOffset   = 1.675;
        $fssOrbitalSlotStart = 60.0;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
        $activeDeviceFlag = "CEPT_HIGH";
    } elsif ($runIdx == 212) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_is_35e_high";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $fssGonTMethod       = "gxt";
        $fssGonTFile         = "IS-35e_C01.gxt";
        $fssGonTFileOffset   = 16.39;
        $fssOrbitalSlotStart = -34.5;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
        $activeDeviceFlag = "CEPT_HIGH";
    } elsif ($runIdx == 213) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_is_14_high";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $fssGonTMethod       = "gxt";
        $fssGonTFile         = "IS-14_CEUH.gxt";
        $fssGonTFileOffset   = 6.29;
        $fssOrbitalSlotStart = -45.0;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
        $activeDeviceFlag = "CEPT_HIGH";
    } elsif ($runIdx == 214) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_is_22_high";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $fssGonTMethod       = "gxt";
        $fssGonTFile         = "IS-22_WHLU.gxt";
        $fssGonTFileOffset   = 4.30;
        $fssOrbitalSlotStart = 72.0;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
        $activeDeviceFlag = "CEPT_HIGH";
    } elsif ($runIdx == 215) {
        $ulsFile = "FS_Database_UK.csv";
        $numTrial = 8000;
        $name = "UK_8000x_mid";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_MID";
    } elsif ($runIdx == 216) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_20w_zone_mid";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $fssGonTMethod       = "gxt";
        $fssGonTFile         = "20W_zone.gxt";
        $fssGonTFileOffset   = 7.80;
        $fssOrbitalSlotStart = -20.0;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
        $activeDeviceFlag = "CEPT_MID";
    } elsif ($runIdx == 217) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_50.5e_zone_mid";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $fssGonTMethod       = "gxt";
        $fssGonTFile         = "50.5E_zone.gxt";
        $fssGonTFileOffset   = 8.4;
        $fssOrbitalSlotStart = 50.5;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
        $activeDeviceFlag = "CEPT_MID";
    } elsif ($runIdx == 218) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_57e_hemispheric_mid";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $fssGonTMethod       = "gxt";
        $fssGonTFile         = "57E_Hemispheric.gxt";
        $fssGonTFileOffset   = 5.1;
        $fssOrbitalSlotStart = 57;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
        $activeDeviceFlag = "CEPT_MID";
    } elsif ($runIdx == 219) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_37.5w_hemispheric_mid";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $fssGonTMethod       = "gxt";
        $fssGonTFile         = "37.5W_Hemispheric.gxt";
        $fssGonTFileOffset   = 7.7;
        $fssOrbitalSlotStart = -37.5;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
        $activeDeviceFlag = "CEPT_MID";
    } elsif ($runIdx == 220) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_20w_zone_high";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $fssGonTMethod       = "gxt";
        $fssGonTFile         = "20W_zone.gxt";
        $fssGonTFileOffset   = 7.80;
        $fssOrbitalSlotStart = -20.0;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
        $activeDeviceFlag = "CEPT_HIGH";
    } elsif ($runIdx == 221) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_50.5e_zone_high";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $fssGonTMethod       = "gxt";
        $fssGonTFile         = "50.5E_zone.gxt";
        $fssGonTFileOffset   = 8.4;
        $fssOrbitalSlotStart = 50.5;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
        $activeDeviceFlag = "CEPT_HIGH";
    } elsif ($runIdx == 222) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_57e_hemispheric_high";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $fssGonTMethod       = "gxt";
        $fssGonTFile         = "57E_Hemispheric.gxt";
        $fssGonTFileOffset   = 5.1;
        $fssOrbitalSlotStart = 57;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
        $activeDeviceFlag = "CEPT_HIGH";
    } elsif ($runIdx == 223) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_37.5w_hemispheric_high";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $fssGonTMethod       = "gxt";
        $fssGonTFile         = "37.5W_Hemispheric.gxt";
        $fssGonTFileOffset   = 7.7;
        $fssOrbitalSlotStart = -37.5;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
        $activeDeviceFlag = "CEPT_HIGH";
    } elsif ($runIdx == 224) {
        $ulsFile = "FS_Database_UK_Top60IovNs.csv";
        $numTrial = 20000;
        $name = "UKtop60_20000x_mid";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_MID";
    } elsif ($runIdx == 225) {
        $ulsFile = "FS_Database_UK.csv";
        $numTrial = 10000;
        $name = "UK_10Kx_mid";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_MID";
    } elsif ($runIdx == 226) {
        $ulsFile = "FS_Database_Netherlands_selecthighint.csv";
        $numTrial = 500000;
        $name = "NL_selecthighint_500Kx_mid";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_MID";
    } elsif ($runIdx == 227) {
        $ulsFile = "FS_Database_UK_selecthighint.csv";
        $numTrial = 500000;
        $name = "UK_selecthighint_500Kx_mid";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_MID";
    } elsif ($runIdx == 228) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_ses_40.5w_hemispheric_mid";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $fssGonTMethod       = "gxt";
        $fssGonTFile         = "40.5W_Hemispheric_EU_Beam_Only.gxt";
        $fssGonTFileOffset   = -0.8;
        $fssOrbitalSlotStart = -40.5;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
        $activeDeviceFlag = "CEPT_MID";
    } elsif ($runIdx == 229) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_ses_40.5w_hemispheric_high";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $fssGonTMethod       = "gxt";
        $fssGonTFile         = "40.5W_Hemispheric_EU_Beam_Only.gxt";
        $fssGonTFileOffset   = -0.8;
        $fssOrbitalSlotStart = -40.5;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
        $activeDeviceFlag = "CEPT_HIGH";
    } elsif ($runIdx == 230) {
        $ulsFile = "FS_Database_UK_selecthighint_27.csv";
        $numTrial = 500000;
        $name = "UK_selecthighint_27_500Kx_mid";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_MID";
    } elsif ($runIdx == 231) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_ses_40.5w_hemispheric_west_mid";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "AC";
        $fssGonTMethod       = "gxt";
        $fssGonTFile         = "40.5W_Hemispheric_West_Beam.gxt";
        $fssGonTFileOffset   = -0.8;
        $fssOrbitalSlotStart = -40.5;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
        $activeDeviceFlag = "CEPT_MID";
    } elsif ($runIdx == 232) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_ses_40.5w_hemispheric_west_high";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "AC";
        $fssGonTMethod       = "gxt";
        $fssGonTFile         = "40.5W_Hemispheric_West_Beam.gxt";
        $fssGonTFileOffset   = -0.8;
        $fssOrbitalSlotStart = -40.5;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
        $activeDeviceFlag = "CEPT_HIGH";
    } elsif ($runIdx == 233) {
        $ulsFile = "FS_Database_UK_singleuls.csv";
        $numTrial = 100000;
        $name = "UK_importance_sampling";
        $analysisMethod = "IMPORTANCE_SAMPLING";
        $pathLossModel = "P.452";
        $closeInPathLossModel = "WINNER2";
        $closeInHgtFlag = 1;
        $closeInHgtLOS = 15.0;
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_MID";

        $ISNumRLAN                = 4;
        $ISAngleCoeffDeg          = 10.0;
        $ISDistCoeffKm            = 1.0;
        $ISW2UrbLOSShiftMean      = -1.5;
        $ISW2UrbNLOSShiftMean     = -1.5;
        $ISW2SubLOSShiftMean      = -1.5;
        $ISW2SubNLOSShiftMean     = -1.5;
        $ISP452Eps                = 0.5;
    } elsif ($runIdx == 234) {
        $ulsFile = "FS_Database_UK_singleuls.csv";
        $numTrial = 10000;
        $name = "UK_singleuls_mc";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_MID";
    } elsif ($runIdx <= 235 + 17 - 1) {
        $ulsFile = "";
        $numTrial = 10;
        my $fssIdx = $runIdx - 235;
        my $antIdx;
        my $antStr;
        $antIdx = $fssIdx + 2;
        if ($antIdx >= 17) { $antIdx = 17; }
        if ($antIdx <= 9) {
            $antStr = "C0${antIdx}";
        } else {
            $antStr = "C${antIdx}";
        }
        if (($fssIdx <= 5) || ($fssIdx == 15)) {
            $region = "EU";
        } else {
            $region = "AC";
        }

        $name = "wifi_to_fss_is_35e_${antStr}_mid_${region}";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $fssGonTMethod       = "gxt";
        $fssGonTFile         = "IS-35e_${antStr}.gxt";
        $fssGonTFileOffset   = 16.39;
        $fssOrbitalSlotStart = -34.5;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
        $activeDeviceFlag = "CEPT_MID";

    } elsif ($runIdx == 252) {
        $ulsFile = "FS_Database_UK_532.csv";
        $numTrial = 1;
        $name = "UK_1x_uls532_mid_showallrlan";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_MID";
        $visibilityThreshold = -10000.0;
    } elsif ($runIdx == 253) {
        $ulsFile = "FS_Database_Netherlands_5.csv";
        $numTrial = 1;
        $name = "NL_1x_uls5_mid_showallrlan";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_MID";
        $visibilityThreshold = -10000.0;
    } elsif ($runIdx == 254) {
        $ulsFile = "FS_Database_Netherlands.csv";
        $numTrial = 10000;
        $name = "NL_10Kx_mid_1411";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_MID";
        $closeInPathLossModel = "P.1411-4.2.1";
    } elsif ($runIdx == 255) {
        $ulsFile = "FS_Database_UK.csv";
        $numTrial = 1000;
        $name = "UK_1000x_mid_1411";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_MID";
        $closeInPathLossModel = "P.1411-4.2.1";
    } elsif ($runIdx == 256) {
        $ulsFile = "FS_Database_Netherlands_8.csv";
        $numTrial = 1;
        $name = "NL_1x_uls8_mid_showallrlan";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_MID";
        $visibilityThreshold = -10000.0;
    } elsif ($runIdx == 257) {
        $ulsFile = "FS_Database_Netherlands.csv";
        $numTrial = 10000;
        $name = "NL_10Kx_mid_1411_LOS";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_MID";
        $closeInPathLossModel = "P.1411-4.2.1LOS";
    } elsif ($runIdx == 258) {
        $ulsFile = "FS_Database_UK.csv";
        $numTrial = 10000;
        $name = "UK_10Kx_mid_1411_LOS";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_MID";
        $closeInPathLossModel = "P.1411-4.2.1LOS";
    } elsif ($runIdx == 259) {
        $ulsFile = "FS_Database_Netherlands.csv";
        $numTrial = 250000;
        $name = "NL_250Kx_W2";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_HIGH";
        $closeInPathLossModel = "WINNER2";
        $closeInHgtFlag = 1;
        $closeInHgtLOS = 15.0;
        $closeInDist = 1000;
        $rlanScaleStr = "_S:0.398724212364,_M:0.64,_L:1.0";
        $runMitigation = "true";
        $mtgnRLANTxPwrDBW = -7.0;
        $heatmapFull = "false";
    } elsif ($runIdx == 260) {
        $ulsFile = "FS_Database_UK.csv";
        $numTrial = 1000;
        $name = "UK_1000x_mid_W2";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_MID";
        $closeInPathLossModel = "WINNER2";
        $closeInHgtFlag = 1;
        $closeInHgtLOS = 15.0;
        $closeInDist = 1000;
    } elsif ($runIdx == 261) {
        $ulsFile = "FS_Database_Netherlands_selecthighint.csv";
        $numTrial = 50000;
        $name = "NL_selecthighint_50Kx_mid_is";
        $analysisMethod = "IMPORTANCE_SAMPLING";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_MID";

        $ISAngleCoeffDeg          = 5500.0;
        $ISDistCoeffKm            = 2750.0;
        $ISP2135UrbLOSShiftMean   = -0.05;
        $ISP2135UrbNLOSShiftMean  = -0.05;
        $ISP2135SubLOSShiftMean   = -0.05;
        $ISP2135SubNLOSShiftMean  = -0.05;
        $ISP452Eps                = 0.01;
    } elsif ($runIdx == 262) {
        $ulsFile = "FS_Database_UK_selecthighint.csv";
        $numTrial = 50000;
        $name = "UK_selecthighint_50Kx_mid_is";
        $analysisMethod = "IMPORTANCE_SAMPLING";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_MID";

        $ISAngleCoeffDeg          = 10000.0;
        $ISDistCoeffKm            = 5000.0;
        $ISP2135UrbLOSShiftMean   = -0.05;
        $ISP2135UrbNLOSShiftMean  = -0.05;
        $ISP2135SubLOSShiftMean   = -0.05;
        $ISP2135SubNLOSShiftMean  = -0.05;
        $ISP452Eps                = 0.01;
    } elsif ($runIdx == 263) {
        $ulsFile = "FS_Database_UK.csv";
        $numTrial = 250000;
        $name = "UK_250Kx_W2";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_HIGH";
        $closeInPathLossModel = "WINNER2";
        $closeInHgtFlag = 1;
        $closeInHgtLOS = 15.0;
        $closeInDist = 1000;
        $rlanScaleStr = "_S:0.398724212364,_M:0.64,_L:1.0";
        $runMitigation = "true";
        $mtgnRLANTxPwrDBW = -7.0;
        $heatmapFull = "false";
    } elsif ($runIdx == 264) {
        $ulsFile = "FS_Database_Netherlands_20.csv";
        $numTrial = 1000;
        $name = "NL_1Kx_uls20_mid_showcloseinrlan";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_MID";
        $closeInFileFlag = 1;
    } elsif ($runIdx == 265) {
        $ulsFile = "FS_Database_Netherlands_12.csv";
        $numTrial = 1;
        $name = "NL_1x_uls12_mid_showallrlan";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_MID";
        $visibilityThreshold = -10000.0;
    } elsif ($runIdx == 266) {
        $ulsFile = "FS_Database_UK_533.csv";
        $numTrial = 1000;
        $name = "UK_1Kx_uls533_mid_showcloseinrlan";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_MID";
        $closeInFileFlag = 1;
    } elsif ($runIdx == 267) {
        $ulsFile = "FS_Database_Netherlands_selecthighint_180915.csv";
        $numTrial = 50000;
        $name = "NL_selecthighint_180915_50Kx_mid_is";
        $analysisMethod = "IMPORTANCE_SAMPLING";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_MID";

        $ISNumRLAN                = 4;
        $ISAngleCoeffDeg          = 10.0;
        $ISDistCoeffKm            = 1.0;
        $ISW2UrbLOSShiftMean      = -1.5;
        $ISW2UrbNLOSShiftMean     = -1.5;
        $ISW2SubLOSShiftMean      = -1.5;
        $ISW2SubNLOSShiftMean     = -1.5;
        $ISP452Eps                = 0.5;
    } elsif ($runIdx == 268) {
        $ulsFile = "FS_Database_UK_selecthighint_180915.csv";
        $numTrial = 50000;
        $name = "UK_selecthighint_180915_50Kx_mid_is";
        $analysisMethod = "IMPORTANCE_SAMPLING";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_MID";

        $ISNumRLAN                = 4;
        $ISAngleCoeffDeg          = 10.0;
        $ISDistCoeffKm            = 1.0;
        $ISW2UrbLOSShiftMean      = -1.5;
        $ISW2UrbNLOSShiftMean     = -1.5;
        $ISW2SubLOSShiftMean      = -1.5;
        $ISW2SubNLOSShiftMean     = -1.5;
        $ISP452Eps                = 0.5;
    } elsif ($runIdx == 269) {
        $ulsFile = "FS_Database_Netherlands_selecthighint_180915.csv";
        $numTrial = 500000;
        $name = "NL_selecthighint_180915_500Kx_mid";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_MID";
    } elsif ($runIdx == 270) {
        $ulsFile = "FS_Database_UK_selecthighint_180915.csv";
        $numTrial = 500000;
        $name = "UK_selecthighint_180915_500Kx_mid";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_MID";
    } elsif ($runIdx == 271) {
        $ulsFile = "FS_Database_UK_532.csv";
        $numTrial = 500000;
        $name = "UK_selecthighint_532_500Kx_mid";
        $analysisMethod = "IMPORTANCE_SAMPLING";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_MID";
        $ISNumRLAN                = 0;
    } elsif ($runIdx == 272) {
        $ulsFile = "FS_Database_Netherlands.csv";
        $numTrial = 10000;
        $name = "NL_10Kx_high_W2";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_HIGH";
        $closeInPathLossModel = "WINNER2";
        $closeInHgtFlag = 1;
        $closeInHgtLOS = 15.0;
        $closeInDist = 1000;
    } elsif ($runIdx == 273) {
        $ulsFile = "FS_Database_UK.csv";
        $numTrial = 10000;
        $name = "UK_10Kx_high_W2";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_HIGH";
        $closeInPathLossModel = "WINNER2";
        $closeInHgtFlag = 1;
        $closeInHgtLOS = 15.0;
        $closeInDist = 1000;
    } elsif ($runIdx == 274) {
        $ulsFile = "output-UL20172211820160-filteredv2.csv";
        # $ulsFile = "jacksonville.csv";
        # $ulsFile = "newyork.csv";
        $name = "Lidar_Analysis";
        $lidarBeamAttnDB = 3.0;
        $analysisMethod = "LIDAR_ANALYSIS";
        $pathLossModel = "UNKNOWN";
        $terrainMethod = "LIDAR";
        $region = "conus";
    } elsif ($runIdx == 275) {
        $ulsFile = "";
        $numTrial = 100000;
        $name = "dbg_winner2";
        $analysisMethod = "DBG_WINNER2";
        $pathLossModel = "UNKNOWN";
    } elsif ($runIdx == 276) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_ses_4_22w_low";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $fssGonTMethod       = "gxt";
        $fssGonTFile         = "22W_Hemispheric.gxt";
        $fssGonTFileOffset   = 5.4;
        $fssOrbitalSlotStart = -22.0;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
        $activeDeviceFlag = "CEPT_LOW";
    } elsif ($runIdx == 277) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_intelsat_33e_60e_low";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $fssGonTMethod       = "csv";
        $fssGonTFile         = "intelsat_33e_60e_gt.csv";
        $fssGonTFileOffset   = 1.675;
        $fssOrbitalSlotStart = 60.0;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
        $activeDeviceFlag = "CEPT_LOW";
    } elsif ($runIdx == 278) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_is_35e_low";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $fssGonTMethod       = "gxt";
        $fssGonTFile         = "IS-35e_C01.gxt";
        $fssGonTFileOffset   = 16.39;
        $fssOrbitalSlotStart = -34.5;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
        $activeDeviceFlag = "CEPT_LOW";
    } elsif ($runIdx == 279) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_is_14_low";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $fssGonTMethod       = "gxt";
        $fssGonTFile         = "IS-14_CEUH.gxt";
        $fssGonTFileOffset   = 6.29;
        $fssOrbitalSlotStart = -45.0;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
        $activeDeviceFlag = "CEPT_LOW";
    } elsif ($runIdx == 280) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_is_22_low";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $fssGonTMethod       = "gxt";
        $fssGonTFile         = "IS-22_WHLU.gxt";
        $fssGonTFileOffset   = 4.30;
        $fssOrbitalSlotStart = 72.0;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
        $activeDeviceFlag = "CEPT_LOW";
    } elsif ($runIdx == 281) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_20w_zone_low";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $fssGonTMethod       = "gxt";
        $fssGonTFile         = "20W_zone.gxt";
        $fssGonTFileOffset   = 7.80;
        $fssOrbitalSlotStart = -20.0;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
        $activeDeviceFlag = "CEPT_LOW";
    } elsif ($runIdx == 282) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_50.5e_zone_low";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $fssGonTMethod       = "gxt";
        $fssGonTFile         = "50.5E_zone.gxt";
        $fssGonTFileOffset   = 8.4;
        $fssOrbitalSlotStart = 50.5;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
        $activeDeviceFlag = "CEPT_LOW";
    } elsif ($runIdx == 283) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_57e_hemispheric_low";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $fssGonTMethod       = "gxt";
        $fssGonTFile         = "57E_Hemispheric.gxt";
        $fssGonTFileOffset   = 5.1;
        $fssOrbitalSlotStart = 57;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
        $activeDeviceFlag = "CEPT_LOW";
    } elsif ($runIdx == 284) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_37.5w_hemispheric_low";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $fssGonTMethod       = "gxt";
        $fssGonTFile         = "37.5W_Hemispheric.gxt";
        $fssGonTFileOffset   = 7.7;
        $fssOrbitalSlotStart = -37.5;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
        $activeDeviceFlag = "CEPT_LOW";
    } elsif ($runIdx == 285) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_ses_40.5w_hemispheric_low";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $fssGonTMethod       = "gxt";
        $fssGonTFile         = "40.5W_Hemispheric_EU_Beam_Only.gxt";
        $fssGonTFileOffset   = -0.8;
        $fssOrbitalSlotStart = -40.5;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
        $activeDeviceFlag = "CEPT_LOW";
    } elsif ($runIdx == 286) {
        $ulsFile = "";
        $numTrial = 10;
        $name = "wifi_to_fss_ses_40.5w_hemispheric_west_low";
        $analysisMethod = "RLAN_TO_GEO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "AC";
        $fssGonTMethod       = "gxt";
        $fssGonTFile         = "40.5W_Hemispheric_West_Beam.gxt";
        $fssGonTFileOffset   = -0.8;
        $fssOrbitalSlotStart = -40.5;
        $fssOrbitalSlotIncr  = 2.0;
        $fssNumOrbitalSlot   = 1;
        $activeDeviceFlag = "CEPT_LOW";
    } elsif ($runIdx == 287) {
        $name = "DBG_CLUTTER_LOSS";
        $analysisMethod = "DBG_CLUTTER_LOSS";
        $pathLossModel = "UNKNOWN";
        $ulsFile = "NONE";
        $numTrial = 10000;
    } elsif ($runIdx == 288) {
        $ulsFile = "FS_Database_UK_531_533.csv";
        $numTrial = 250000;
        $name = "UK_250Kx_W2_531_533";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_HIGH";
        $closeInPathLossModel = "WINNER2";
        $closeInHgtFlag = 1;
        $closeInHgtLOS = 15.0;
        $closeInDist = 1000;
        $rlanScaleStr = "_S:0.398724212364,_M:0.64,_L:1.0";
        $runMitigation = "true";
        $mtgnRLANTxPwrDBW = -7.0;
        $heatmapFull = "false";
    } elsif ($runIdx == 289) {
        $ulsFile = "";
        $name = "Lidar_Info";
        $analysisMethod = "LIDAR_INFO";
        $pathLossModel = "UNKNOWN";
        $terrainMethod = "UNKNOWN";
    } elsif ($runIdx == 290) {
        $ulsFile = "20190123_database_103_toMike.csv";
        # $ulsFile = "20190705_database_103_toMike.csv";
        # $ulsFile = "output-UL20172211820160-filteredv2.csv";
        $removeMobile = 1;
        $name = "FS_Info";
        $analysisMethod = "FS_INFO";
        $pathLossModel = "UNKNOWN";
        $terrainMethod = "UNKNOWN";
    } elsif ($runIdx == 291) {
        # $ulsFile = "output-UL20172211820160-filteredv2.csv";
        # $ulsFile = "ULS_06May2019_6GHz.csv";
        $ulsFile = "ULS_23Jan2019_perlink.csv";
        $lidarExtractULSFlag = "true";
        $name = "lidar_new_york";
        $lidarDataFileList = ( ($hostname eq "wellington") ? "/mnt/usb/lidar_alionnew/New_York_NY" :
                               ($hostname eq "yarmouth"  ) ? "/mnt/usb/lidar_alionnew/New_York_NY" : "New_York_NY");
        $lidarBeamAttnDB = 3.0;
        $analysisMethod = "LIDAR_ANALYSIS";
        $pathLossModel = "UNKNOWN";
        $terrainMethod = "LIDAR";
        $region = "conus";
    } elsif ($runIdx == 292) {
        # $ulsFile = "output-UL20172211820160-filteredv2.csv";
        # $ulsFile = "ULS_06May2019_6GHz.csv";
        $ulsFile = "ULS_23Jan2019_perlink.csv";
        $name = "extract_uls_city";
        $runType = "extract_uls_citylist";
        $lidarExtractULSFlag = "true";
        @cityList = ( "/mnt/usb/lidar_alionnew/New_York_NY",
                      "/mnt/usb/lidar_181219/Los_Angeles_2007_MSL",
                      "/mnt/usb/lidar_alionnew/Chicago_IL",
                      "/mnt/usb/lidar_alionnew/Dallas_TX",
                      "/mnt/usb/lidar_alionnew/Houston_TX",
                      "/mnt/usb/directlidar/Washington_DC",
                      "/mnt/usb/lidar_alionnew/Miami_FL",
                      "/mnt/usb/lidar_alionnew/Philadelphia_PA",
                      "/mnt/usb/lidar_alionnew/Atlanta_GA",
                      "/mnt/usb/lidar_alionnew/Boston_MA" );

        $lidarBeamAttnDB = 3.0;
        $analysisMethod = "LIDAR_ANALYSIS";
        $pathLossModel = "UNKNOWN";
        $terrainMethod = "LIDAR";
        $region = "conus";
    } elsif ($runIdx == 293) {
        $name = "lidar_top10";
        $runType = "lidar_citylist";
        @cityList = ( "/mnt/usb/lidar_alionnew/Dallas_TX:dallas_blacklist.csv",
                      "/mnt/usb/lidar_alionnew/New_York_NY:new_york_blacklist.csv",
                      "/mnt/usb/lidar_181219/Los_Angeles_2007_MSL:",
                      "/mnt/usb/lidar_alionnew/Chicago_IL:",
                      "/mnt/usb/lidar_alionnew/Houston_TX:",
                      "/mnt/usb/directlidar/Washington_DC:",
                      "/mnt/usb/lidar_alionnew/Miami_FL:",
                      "/mnt/usb/lidar_alionnew/Philadelphia_PA:",
                      "/mnt/usb/lidar_alionnew/Atlanta_GA:",
                      "/mnt/usb/lidar_alionnew/Boston_MA:" );

        $lidarMinOcclusionHt = 50.0;
        $lidarBeamAttnDB = 3.0;
        $analysisMethod = "LIDAR_ANALYSIS";
        $pathLossModel = "UNKNOWN";
        $terrainMethod = "LIDAR";
        $region = "conus";
    } elsif ($runIdx == 294) {
        # $ulsFile = "newyork_ULS_45Tiles_corrected.csv";
        # $ulsFile = "newyork_ULS_45Tiles_correctedv2.csv";
        # $ulsFile = "newyork_ULS_45Tiles_correctedv3.csv";
        # $ulsFile = "newyork_1951.csv";
        $ulsFile = "newyork_190510.csv";
        $lidarBlacklistFile = "new_york_blacklist.csv";
        $name = "lidar_new_york_fixed_uls_loc";
        $lidarDataFileList = ( ($hostname eq "wellington") ? "/mnt/usb/lidar_alionnew/New_York_NY" :
                               ($hostname eq "yarmouth"  ) ? "/mnt/usb/lidar_alionnew/New_York_NY" : "New_York_NY");
        $lidarMinOcclusionHt = 50.0;
        $lidarBeamAttnDB = 3.0;
        $analysisMethod = "LIDAR_ANALYSIS";
        $pathLossModel = "UNKNOWN";
        $terrainMethod = "LIDAR";
        $region = "conus";
    } elsif ($runIdx == 295) {
        $ulsFile = "ULS_23Jan2019_v3_Filtered_to_only_6_GHz_Band_all.csv";
        $numTrial = 1;
        $name = "conus_10x";
        $adjustSimulationRegion = "false";
        $analysisMethod = "MONTE_CARLO";
        $filterSimRegionOnly = "true";
    } elsif ($runIdx == 296) {
        $ulsFile = "ULS_23Jan2019_v3_Filtered_to_only_6_GHz_Band_withFixMods_fixbps_sort.csv";
        $numTrial = 1;
        $name = "conus_10x";
        $adjustSimulationRegion = "false";
        $analysisMethod = "MONTE_CARLO";
        $filterSimRegionOnly = "true";
    } elsif ($runIdx == 297) {
        $ulsFile = "FS_Database_UK.csv";
        $numTrial = 1;
        $name = "UK_pl_20_50";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_HIGH";
        $closeInPathLossModel = "WINNER2";
        $closeInHgtFlag = 1;
        $closeInHgtLOS = 15.0;
        $closeInDist = 1000;
        $visibilityThreshold = -10000.0;
        $compute_pl_20_50_flag = "true";
    } elsif ($runIdx == 298) {
        $ulsFile = "FS_Database_French.csv";
        $ulsAntennaPatternFile = "france_antennas.csv";
        $maxRadius = 75.0e3;
        $numTrial = 10000;
        $name = "FR_10Kx_75km";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_MID";
        $closeInPathLossModel = "WINNER2";
        $closeInHgtFlag = 1;
        $closeInHgtLOS = 15.0;
        $closeInDist = 1000;
    } elsif ($runIdx == 299) {
        $ulsFile = "FS_Database_French.csv";
        $ulsAntennaPatternFile = "france_antennas.csv";
        $maxRadius = 150.0e3;
        $numTrial = 10000;
        $name = "FR_10Kx_150km";
        $analysisMethod = "MONTE_CARLO";
        $pathLossModel = "P.452";
        $clutterFile = "g100_clc12_V18_5.tif";
        $eirpOption = "baseline_eu";
        $region = "EU";
        $activeDeviceFlag = "CEPT_MID";
        $closeInPathLossModel = "WINNER2";
        $closeInHgtFlag = 1;
        $closeInHgtLOS = 15.0;
        $closeInDist = 1000;
    } elsif ($runIdx == 300) {
        $ulsFile = "output-UL20172211820160-filteredv2.csv";
        $name = "cvg_cone_new_york";
        $lidarCvgConeOnly = "true";
        $lidarBeamAttnDB = 3.0;
        $analysisMethod = "LIDAR_ANALYSIS";
        $pathLossModel = "UNKNOWN";
        $region = "conus";

        # Covers 50 Km radius centered at (lat = 40-47-00 N, long = 73-58-00 W)
        $fsCustomFilterFlag = "true";
        $fsCustomFilterLonMin = -74.6;
        $fsCustomFilterLonMax = -73.3;
        $fsCustomFilterLatMin = 40.3;
        $fsCustomFilterLatMax = 41.3;

    } elsif ($runIdx <= 301 + 6 - 1) {
        my $sIdx = $runIdx - 301;

        $ulsFile = "ULS_23Jan2019_perlink.csv";
        $numTrial = 1;
        $name = "dbg_conus_1x_s${sIdx}";
        $adjustSimulationRegion = "false";
        $analysisMethod = "MONTE_CARLO";
        $visibilityThreshold = -10.0;
        $useMultiThreading = "true";

        $nearFieldLossFlag = "true";

        if ($sIdx < 1) { $pathLossModel = "FSPL"; }
        if ($sIdx < 2) { $noBuildingPenetrationLossFlag = "true"; }
        if ($sIdx < 3) { $eirpOption = "max"; }
        if ($sIdx < 4) { $noPolarizationLossFlag = "true"; }
        if ($sIdx < 5) { $probIndoorBodyLossRLAN  = "100.0:0";
                         $probOutdoorBodyLossRLAN = "100.0:0"; }

    } elsif ($runIdx == 307) {
        $name = "dbg_near_field";
        $analysisMethod = "DBG_NEAR_FIELD";
        $terrainMethod = "UNKNOWN";
    } elsif ($runIdx <= 308 + 4 - 1) {
        my $sIdx = $runIdx - 308;
        my $beamWidth = 3*($sIdx+1);
        $ulsFile = "dallas_190510.csv";
        $name = "cvg_cone_dallas_bw_${beamWidth}dB";
        $lidarDataFileList = ( ($hostname eq "wellington") ? "/mnt/usb/lidar_alionnew/Dallas_TX" :
                               ($hostname eq "yarmouth"  ) ? "/mnt/usb/lidar_alionnew/Dallas_TX" : "Dallas_TX");
        $lidarCvgConeOnly = "true";
        $lidarBeamAttnDB = ${beamWidth};
        $analysisMethod = "LIDAR_ANALYSIS";
        $pathLossModel = "UNKNOWN";
        $terrainMethod = "LIDAR";
        $region = "conus";

    } elsif ($runIdx == 312) {
        $ulsFile = "newyork_WPNA240.csv";
        $numTrial = 10;
        $name = "NY_${numTrial}x_WPNA240_showallrlan";
        $analysisMethod = "MONTE_CARLO";
        $visibilityThreshold = -10000.0;
    } elsif ($runIdx == 313) {
        $ulsFile = "losangeles_WQJS776.csv";
        $numTrial = 1;
        $name = "LA_1x_WQJS776_showallrlan";
        $analysisMethod = "MONTE_CARLO";
        $visibilityThreshold = -10000.0;
    } elsif ($runIdx == 314) {
        $ulsFile = "dallas_WED398.csv";
        $numTrial = 1;
        $name = "DL_1x_WED398_showallrlan";
        $analysisMethod = "MONTE_CARLO";
        $visibilityThreshold = -10000.0;
    } elsif ($runIdx == 315) {
        $ulsFile = "";
        $ulsFile = "ULS_23Jan2019_perlink_fixedwithbas.csv";
        # $ulsFile = "ULS_23Jan2019_perlink.csv";
        $lidarBeamAttnDB = 3.0;
        $name = "nlcd_analysis";
        $analysisMethod = "NLCD_ANALYSIS";
        $pathLossModel = "UNKNOWN";
        $terrainMethod = "UNKNOWN";

    } else {
        die "ERROR: $!\n";
    }

    my $heatmapFile = "${name}_heatmap";
    my $excThrFile = "${name}_exc_thr_wifi";
    my $closeInFile = ($closeInFileFlag ? "${name}_close_in_wifi.csv" : "");
    my $fresnelFile = "${name}_fresnel";
    my $fsHistFile = "${name}_fs_hist.csv";
    my $fsOOBFile = "${name}_fs_oob.csv";
    my $fssLinkBudgetFilePfx = "${name}_fss_link_budget_";
    my $lidarResultFile = "${name}_lidar_result.csv";
    my $lidarCvgFile = "${name}_lidar_cvg.txt";

    my $runFlag;
    $runFlag = ( ($runIdx ==  0) ? 0 :
                 ($runIdx ==  1) ? 0 :
                 ($runIdx ==  2) ? 0 :
                 ($runIdx ==  3) ? 0 :
                 ($runIdx ==  4) ? 0 :
                 ($runIdx ==  5) ? 0 :
                 ($runIdx ==  6) ? 0 :
                 ($runIdx ==  7) ? 0 :
                 ($runIdx ==  8) ? 0 :
                 ($runIdx ==  9) ? 0 :
                 ($runIdx == 10) ? 0 :
                 ($runIdx == 11) ? 0 :
                 ($runIdx == 12) ? 0 :
                 ($runIdx == 13) ? 0 :
                 ($runIdx == 14) ? 0 :
                 ($runIdx == 15) ? 0 :
                 ($runIdx == 16) ? 0 :
                 ($runIdx == 17) ? 0 :
                 ($runIdx == 18) ? 0 :
                 ($runIdx == 19) ? 0 :
                 ($runIdx == 20) ? 0 :
                 ($runIdx == 21) ? 0 :
                 ($runIdx == 22) ? 0 :
                 ($runIdx == 23) ? 0 :
                 ($runIdx == 24) ? 0 :
                 ($runIdx == 25) ? 0 :
                 ($runIdx == 26) ? 0 :
                 ($runIdx == 27) ? 0 :
                 ($runIdx == 28) ? 0 :
                 ($runIdx == 29) ? 0 :
                 ($runIdx == 30) ? 0 :
                 ($runIdx == 31) ? 0 :
                 ($runIdx == 32) ? 0 :
                 ($runIdx == 33) ? 0 :
                 ($runIdx == 34) ? 0 :
                 ($runIdx == 35) ? 0 :
                 ($runIdx == 36) ? 0 :
                 ($runIdx == 37) ? 0 :
                 ($runIdx == 38) ? 0 :
                 ($runIdx == 39) ? 0 :
                 ($runIdx == 40) ? 0 :
                 ($runIdx == 41) ? 0 :
                 ($runIdx == 42) ? 0 :
                 ($runIdx == 43) ? 0 :
                 ($runIdx == 44) ? 0 :
                 ($runIdx == 45) ? 0 :
                 ($runIdx == 46) ? 0 :
                 ($runIdx == 47) ? 0 :
                 ($runIdx == 48) ? 0 :
                 ($runIdx == 49) ? 0 :
                 ($runIdx == 50) ? 0 :
                 ($runIdx == 51) ? 0 :
                 ($runIdx == 52) ? 0 :
                 ($runIdx == 53) ? 0 :
                 ($runIdx == 54) ? 0 :
                 ($runIdx == 55) ? 0 :
                 ($runIdx == 56) ? 0 :
                 ($runIdx == 57) ? 0 :
                 ($runIdx == 58) ? 0 :
                 ($runIdx == 59) ? 0 :
                 ($runIdx == 60) ? 0 :
                 ($runIdx == 61) ? 0 :
                 ($runIdx == 62) ? 0 :
                 ($runIdx == 63) ? 0 :
                 ($runIdx == 64) ? 0 :
                 ($runIdx == 65) ? 0 :
                 ($runIdx == 66) ? 0 :
                 ($runIdx == 67) ? 0 :
                 ($runIdx == 68) ? 0 :
                 ($runIdx == 69) ? 0 :
                 ($runIdx == 70) ? 0 :
                 ($runIdx == 71) ? 0 :
                 ($runIdx == 72) ? 0 :
                 ($runIdx == 73) ? 0 :
                 ($runIdx == 74) ? 0 :
                 ($runIdx == 75) ? 0 :
                 ($runIdx == 76) ? 0 :
                 ($runIdx == 77) ? 0 :
                 (($runIdx >= 78) && ($runIdx <= 78+120-1)) ? ((($runIdx - 78)%3 == 0) ? 0 : 0) :
                 ($runIdx == 198) ? 0 :
                 ($runIdx == 199) ? 0 :
                 ($runIdx == 200) ? 0 :
                 ($runIdx == 201) ? 0 :
                 ($runIdx == 202) ? 0 :
                 ($runIdx == 203) ? 0 :
                 ($runIdx == 204) ? 0 :
                 ($runIdx == 205) ? 0 :
                 ($runIdx == 206) ? 0 :
                 ($runIdx == 207) ? 0 :
                 ($runIdx == 208) ? 0 :
                 ($runIdx == 209) ? 0 :
                 ($runIdx == 210) ? 0 :
                 ($runIdx == 211) ? 0 :
                 ($runIdx == 212) ? 0 :
                 ($runIdx == 213) ? 0 :
                 ($runIdx == 214) ? 0 :
                 ($runIdx == 215) ? 0 :
                 ($runIdx == 216) ? 0 :
                 ($runIdx == 217) ? 0 :
                 ($runIdx == 218) ? 0 :
                 ($runIdx == 219) ? 0 :
                 ($runIdx == 220) ? 0 :
                 ($runIdx == 221) ? 0 :
                 ($runIdx == 222) ? 0 :
                 ($runIdx == 223) ? 0 :
                 ($runIdx == 224) ? 0 :
                 ($runIdx == 225) ? 0 :
                 ($runIdx == 226) ? 0 :
                 ($runIdx == 227) ? 0 :
                 ($runIdx == 228) ? 0 :
                 ($runIdx == 229) ? 0 :
                 ($runIdx == 230) ? 0 :
                 ($runIdx == 231) ? 0 :
                 ($runIdx == 232) ? 0 :
                 ($runIdx == 233) ? 0 :
                 ($runIdx == 234) ? 0 :
                 (($runIdx >= 235) && ($runIdx <= 235+17-1)) ? ((($runIdx - 235)%3 == 0) ? 0 : 0) :
                 ($runIdx == 252) ? 0 :
                 ($runIdx == 253) ? 0 :
                 ($runIdx == 254) ? 0 :
                 ($runIdx == 255) ? 0 :
                 ($runIdx == 256) ? 0 :
                 ($runIdx == 257) ? 0 :
                 ($runIdx == 258) ? 0 :
                 ($runIdx == 259) ? 0 :
                 ($runIdx == 260) ? 0 :
                 ($runIdx == 261) ? 0 :
                 ($runIdx == 262) ? 0 :
                 ($runIdx == 263) ? 0 :
                 ($runIdx == 264) ? 0 :
                 ($runIdx == 265) ? 0 :
                 ($runIdx == 266) ? 0 :
                 ($runIdx == 267) ? 0 :
                 ($runIdx == 268) ? 0 :
                 ($runIdx == 269) ? 0 :
                 ($runIdx == 270) ? 0 :
                 ($runIdx == 271) ? 0 :
                 ($runIdx == 272) ? 0 :
                 ($runIdx == 273) ? 0 :
                 ($runIdx == 274) ? 0 :
                 ($runIdx == 275) ? 0 :
                 ($runIdx == 276) ? 0 :
                 ($runIdx == 277) ? 0 :
                 ($runIdx == 278) ? 0 :
                 ($runIdx == 279) ? 0 :
                 ($runIdx == 280) ? 0 :
                 ($runIdx == 281) ? 0 :
                 ($runIdx == 282) ? 0 :
                 ($runIdx == 283) ? 0 :
                 ($runIdx == 284) ? 0 :
                 ($runIdx == 285) ? 0 :
                 ($runIdx == 286) ? 0 :
                 ($runIdx == 287) ? 0 :
                 ($runIdx == 288) ? 0 :
                 ($runIdx == 289) ? 0 :
                 ($runIdx == 290) ? 0 :
                 ($runIdx == 291) ? 0 :
                 ($runIdx == 292) ? 0 :
                 ($runIdx == 293) ? ($hostname eq "yarmouth" ? 1 : 0)*0 :
                 ($runIdx == 294) ? ($hostname eq "yarmouth" ? 1 : 0)*0 :
                 ($runIdx == 295) ? 0 :
                 ($runIdx == 296) ? 0 :
                 ($runIdx == 297) ? 0 :
                 ($runIdx == 298) ? 0 :
                 ($runIdx == 299) ? 0 :
                 ($runIdx == 300) ? 0 :
                 (($runIdx >= 301) && ($runIdx <= 301+6-1)) ? (($runIdx - 301)%3 == ($hostname eq "wellington" ? 0 : $hostname eq "erik-kelvin.rkf-engineering.com" ? 1 : 2) ? 1 : 0)*0 :
                 ($runIdx == 307) ? 0 :
                 (($runIdx >= 308) && ($runIdx <= 308+4-1)) ? (($runIdx - 308)%4 == 3 ? 1 :0)*0 :
                 ($runIdx == 312) ? 0 :
                 ($runIdx == 313) ? 0 :
                 ($runIdx == 314) ? 0 :
                 ($runIdx == 315) ? 1 :
               0 );

#        if (($runIdx >= 78) && ($runIdx <= 78+120-1)) {
#            my $smIdx = $runIdx - 78;
#            my $addlAttnIdx = $smIdx % 2;
#            $smIdx = ($smIdx - $addlAttnIdx)/2;
#            my $bandIdx = $smIdx % 2;
#            $smIdx = ($smIdx - $bandIdx)/2;
#            my $bldgIdx = $smIdx % 2;
#            $smIdx = ($smIdx - $bldgIdx)/2;
#            my $eirpIdx = $smIdx % 3;
#            $smIdx = ($smIdx - $eirpIdx)/3;
#            my $cityIdx = $smIdx % 5;
#
#            if ( ($cityIdx == 3) && ($addlAttnIdx == 0) && ($bandIdx == 0) && ($bldgIdx == 1) && ($eirpIdx == 2) ) {
#                $runFlag = 1;
#            } else {
#                $runFlag = 0;
#            }
#        }

$caseList->{"case_${caseIdx}"}->{name} = $name;
$caseList->{"case_${caseIdx}"}->{dir} = "sim_170627";
$caseList->{"case_${caseIdx}"}->{numTrial}               = $numTrial;

my $indoorFrac;
my $thFrac;

if ($region eq "conus") {
    $caseList->{"case_${caseIdx}"}->{regionStr}  = "CONUS:0";

    if (0) {
        # $caseList->{"case_${caseIdx}"}->{popDensityFile}  = "gpw4_pop_density_2020_conus.csv";
        $caseList->{"case_${caseIdx}"}->{popDensityFile}  = "gpw4_pop_density_2020_conus_withzeros.csv";
        $caseList->{"case_${caseIdx}"}->{popDensityResLon}  = 0.0166732; # 1.0 / 60;
        $caseList->{"case_${caseIdx}"}->{popDensityResLat}  = 0.0166802; # 1.0 / 60;
        $caseList->{"case_${caseIdx}"}->{popDensityMinLon} = -124.6930;
        $caseList->{"case_${caseIdx}"}->{popDensityNumLon} = 3461;
        $caseList->{"case_${caseIdx}"}->{popDensityMinLat} = 25.1499;
        $caseList->{"case_${caseIdx}"}->{popDensityNumLat} = 1452;
    } else {
        # Edge of region fixed where some of coastline was previously cut off
        $caseList->{"case_${caseIdx}"}->{popDensityFile}  = "conus_1arcmin.csv";
        $caseList->{"case_${caseIdx}"}->{popDensityResLon}  = 1.0 / 60;
        $caseList->{"case_${caseIdx}"}->{popDensityResLat}  = 1.0 / 60;
        $caseList->{"case_${caseIdx}"}->{popDensityMinLon} = -124.7333;
        $caseList->{"case_${caseIdx}"}->{popDensityNumLon} = 3467;
        $caseList->{"case_${caseIdx}"}->{popDensityMinLat} = 24.5333;
        $caseList->{"case_${caseIdx}"}->{popDensityNumLat} = 1491;
    }

    my $yearIdx = 5;
    if ($yearIdx == 0) {
        # Census projections for 2020
        $numRLANUrbanCorporate    = 7536;
        $numRLANUrbanPublic       = 15072;
        $numRLANUrbanHome         = 52752;
        $numRLANSuburbanCorporate = 251;
        $numRLANSuburbanPublic    = 503;
        $numRLANSuburbanHome      = 4273;
        $numRLANRuralCorporate    = 0;
        $numRLANRuralPublic       = 0;
        $numRLANRuralHome         = 4922;
    } elsif ($yearIdx == 1) {
        # Census projections for 2025
        $numRLANUrbanCorporate    = 37680;
        $numRLANUrbanPublic       = 75359;
        $numRLANUrbanHome         = 263758;
        $numRLANSuburbanCorporate = 2514;
        $numRLANSuburbanPublic    = 5028;
        $numRLANSuburbanHome      = 42734;
        $numRLANRuralCorporate    = 0;
        $numRLANRuralPublic       = 0;
        $numRLANRuralHome         = 49217;
    } elsif ($yearIdx == 2) {
        # Census projections for 2025 (updated 2017.08.03)
        $numRLANUrbanCorporate    = 101263;
        $numRLANUrbanPublic       = 151894;
        $numRLANUrbanHome         = 531630;
        $numRLANSuburbanCorporate = 6756;
        $numRLANSuburbanPublic    = 10134;
        $numRLANSuburbanHome      = 86136;
        $numRLANRuralCorporate    = 0;
        $numRLANRuralPublic       = 0;
        $numRLANRuralHome         = 99201;
    } elsif ($yearIdx == 3) {
        # Census projections for 2025 (updated 2017.08.19)
        $numRLANUrbanCorporate    = 60879;
        $numRLANUrbanPublic       = 121757;
        $numRLANUrbanHome         = 426150;
        $numRLANSuburbanCorporate = 4348;
        $numRLANSuburbanPublic    = 8697;
        $numRLANSuburbanHome      = 73924;
        $numRLANRuralCorporate    = 2174;
        $numRLANRuralPublic       = 2174;
        $numRLANRuralHome         = 82621;
        $numRLANBarrenCorporate   = 0;
        $numRLANBarrenPublic      = 0;
        $numRLANBarrenHome        = 86969;
    } elsif ($yearIdx == 4) {
        # Census projections for 2025 (updated 2017.10.11)
        $numRLANUrbanCorporate    = 16708;
        $numRLANUrbanPublic       = 4214;
        $numRLANUrbanHome         = 282771;
        $numRLANSuburbanCorporate = 1193;
        $numRLANSuburbanPublic    = 602;
        $numRLANSuburbanHome      = 42772;
        $numRLANRuralCorporate    = 477;
        $numRLANRuralPublic       = 120;
        $numRLANRuralHome         = 46099;
        $numRLANBarrenCorporate   = 0;
        $numRLANBarrenPublic      = 0;
        $numRLANBarrenHome        = 47525;
    } elsif ($yearIdx == 5) {
        # Census projections for 2025, barren wifi removed from simulation (updated 2017.11.03)
        # Used for US report
        $numRLANUrbanCorporate    = 16708;
        $numRLANUrbanPublic       = 4214;
        $numRLANUrbanHome         = 282771;
        $numRLANSuburbanCorporate = 1193;
        $numRLANSuburbanPublic    = 602;
        $numRLANSuburbanHome      = 42772;
        $numRLANRuralCorporate    = 477;
        $numRLANRuralPublic       = 120;
        $numRLANRuralHome         = 46099;
        $numRLANBarrenCorporate   = 0;
        $numRLANBarrenPublic      = 0;
        $numRLANBarrenHome        = 0;
    } elsif ($yearIdx == 6) {
        # Used for NPRM
        $numRLANUrbanCorporate    = 58922;
        $numRLANUrbanPublic       = 29461;
        $numRLANUrbanHome         = 500838;
        $numRLANSuburbanCorporate = 6313;
        $numRLANSuburbanPublic    = 6313;
        $numRLANSuburbanHome      = 113636;
        $numRLANRuralCorporate    = 2525;
        $numRLANRuralPublic       = 1263;
        $numRLANRuralHome         = 122474;
        $numRLANBarrenCorporate   = 0;
        $numRLANBarrenPublic      = 0;
        $numRLANBarrenHome        = 0;
    } else {
        die "ERROR: $!\n";
    }

    if ($scaleNumRLANConus != 1) {
        $numRLANUrbanCorporate    *= $scaleNumRLANConus;
        $numRLANUrbanPublic       *= $scaleNumRLANConus;
        $numRLANUrbanHome         *= $scaleNumRLANConus;
        $numRLANSuburbanCorporate *= $scaleNumRLANConus;
        $numRLANSuburbanPublic    *= $scaleNumRLANConus;
        $numRLANSuburbanHome      *= $scaleNumRLANConus;
        $numRLANRuralCorporate    *= $scaleNumRLANConus;
        $numRLANRuralPublic       *= $scaleNumRLANConus;
        $numRLANRuralHome         *= $scaleNumRLANConus;
        $numRLANBarrenCorporate   *= $scaleNumRLANConus;
        $numRLANBarrenPublic      *= $scaleNumRLANConus;
        $numRLANBarrenHome        *= $scaleNumRLANConus;
    }

    $caseList->{"case_${caseIdx}"}->{densityThrUrban}        = 486.75e-6;
    $caseList->{"case_${caseIdx}"}->{densityThrSuburban}     = 211.205e-6;
    $caseList->{"case_${caseIdx}"}->{densityThrRural}        = 57.1965e-6;

    $indoorFrac = 1.0-$outdoorFrac;
    $thFrac = 0.2;  # 20 % of bldgs thermally efficient

    $ulsAntennaPatternFile = "";
    $caseList->{"case_${caseIdx}"}->{srtmDir}            = "local_srtm";
} elsif ($region eq "EU") {
    $caseList->{"case_${caseIdx}"}->{regionStr}  = "EU:0,AF:1,ME:2";
if (1) {
    $caseList->{"case_${caseIdx}"}->{popDensityFile}  = "ecc-rlan-study-pop-w-region-v3.csv";
    $caseList->{"case_${caseIdx}"}->{popDensityResLon} = 30.0 / 3600;
    $caseList->{"case_${caseIdx}"}->{popDensityResLat} = 30.0 / 3600;
    $caseList->{"case_${caseIdx}"}->{popDensityMinLon} = -3754.0/120;
    $caseList->{"case_${caseIdx}"}->{popDensityNumLon} = 11702;
    $caseList->{"case_${caseIdx}"}->{popDensityMinLat} = -5636.0/120;
    $caseList->{"case_${caseIdx}"}->{popDensityNumLat} = 15293;
} elsif (0) {
    $caseList->{"case_${caseIdx}"}->{popDensityFile}  = "ecc-rlan-study-pop-w-region-v2.csv";
    $caseList->{"case_${caseIdx}"}->{popDensityResLon} = 30.0 / 3600;
    $caseList->{"case_${caseIdx}"}->{popDensityResLat} = 30.0 / 3600;
    $caseList->{"case_${caseIdx}"}->{popDensityMinLon} = -3754.0/120;
    $caseList->{"case_${caseIdx}"}->{popDensityNumLon} = 11702;
    $caseList->{"case_${caseIdx}"}->{popDensityMinLat} = -5636.0/120;
    $caseList->{"case_${caseIdx}"}->{popDensityNumLat} = 15293;
} else {
    $caseList->{"case_${caseIdx}"}->{popDensityFile}  = "ecc_dbg_pop.csv";
    $caseList->{"case_${caseIdx}"}->{popDensityResLon} = 30.0 / 3600;
    $caseList->{"case_${caseIdx}"}->{popDensityResLat} = 30.0 / 3600;
    $caseList->{"case_${caseIdx}"}->{popDensityMinLon} = -7.0;
    $caseList->{"case_${caseIdx}"}->{popDensityNumLon} = 7*120+1;
    $caseList->{"case_${caseIdx}"}->{popDensityMinLat} = 47.0;
    $caseList->{"case_${caseIdx}"}->{popDensityNumLat} = 7*120+1;
}


    my $numRLANUrbanCorporateEU;
    my $numRLANUrbanPublicEU;
    my $numRLANUrbanHomeEU;
    my $numRLANSuburbanCorporateEU;
    my $numRLANSuburbanPublicEU;
    my $numRLANSuburbanHomeEU;
    my $numRLANRuralCorporateEU;
    my $numRLANRuralPublicEU;
    my $numRLANRuralHomeEU;
    my $numRLANBarrenCorporateEU;
    my $numRLANBarrenPublicEU;
    my $numRLANBarrenHomeEU;

    my $numRLANUrbanCorporateAF;
    my $numRLANUrbanPublicAF;
    my $numRLANUrbanHomeAF;
    my $numRLANSuburbanCorporateAF;
    my $numRLANSuburbanPublicAF;
    my $numRLANSuburbanHomeAF;
    my $numRLANRuralCorporateAF;
    my $numRLANRuralPublicAF;
    my $numRLANRuralHomeAF;
    my $numRLANBarrenCorporateAF;
    my $numRLANBarrenPublicAF;
    my $numRLANBarrenHomeAF;

    my $numRLANUrbanCorporateME;
    my $numRLANUrbanPublicME;
    my $numRLANUrbanHomeME;
    my $numRLANSuburbanCorporateME;
    my $numRLANSuburbanPublicME;
    my $numRLANSuburbanHomeME;
    my $numRLANRuralCorporateME;
    my $numRLANRuralPublicME;
    my $numRLANRuralHomeME;
    my $numRLANBarrenCorporateME;
    my $numRLANBarrenPublicME;
    my $numRLANBarrenHomeME;

    if (1) {
        if ($activeDeviceFlag eq "CEPT_MID") {
            $numRLANUrbanCorporateEU    = 0;
            $numRLANUrbanPublicEU       = 0;
            $numRLANUrbanHomeEU         = 658517;
            $numRLANSuburbanCorporateEU = 0;
            $numRLANSuburbanPublicEU    = 0;
            $numRLANSuburbanHomeEU      = 355599;
            $numRLANRuralCorporateEU    = 0;
            $numRLANRuralPublicEU       = 0;
            $numRLANRuralHomeEU         = 302918;
            $numRLANBarrenCorporateEU   = 0;
            $numRLANBarrenPublicEU      = 0;
            $numRLANBarrenHomeEU        = 0;

            $numRLANUrbanCorporateAF    = 0;
            $numRLANUrbanPublicAF       = 0;
            $numRLANUrbanHomeAF         = 301561;
            $numRLANSuburbanCorporateAF = 0;
            $numRLANSuburbanPublicAF    = 0;
            $numRLANSuburbanHomeAF      = 162843;
            $numRLANRuralCorporateAF    = 0;
            $numRLANRuralPublicAF       = 0;
            $numRLANRuralHomeAF         = 138718;
            $numRLANBarrenCorporateAF   = 0;
            $numRLANBarrenPublicAF      = 0;
            $numRLANBarrenHomeAF        = 0;

            $numRLANUrbanCorporateME    = 0;
            $numRLANUrbanPublicME       = 0;
            $numRLANUrbanHomeME         = 84983;
            $numRLANSuburbanCorporateME = 0;
            $numRLANSuburbanPublicME    = 0;
            $numRLANSuburbanHomeME      = 45891;
            $numRLANRuralCorporateME    = 0;
            $numRLANRuralPublicME       = 0;
            $numRLANRuralHomeME         = 39092;
            $numRLANBarrenCorporateME   = 0;
            $numRLANBarrenPublicME      = 0;
            $numRLANBarrenHomeME        = 0;
        } elsif ($activeDeviceFlag eq "CEPT_LOW") {
            $numRLANUrbanCorporateEU    = 0;
            $numRLANUrbanPublicEU       = 0;
            $numRLANUrbanHomeEU         = 410260;
            $numRLANSuburbanCorporateEU = 0;
            $numRLANSuburbanPublicEU    = 0;
            $numRLANSuburbanHomeEU      = 221541;
            $numRLANRuralCorporateEU    = 0;
            $numRLANRuralPublicEU       = 0;
            $numRLANRuralHomeEU         = 188720;
            $numRLANBarrenCorporateEU   = 0;
            $numRLANBarrenPublicEU      = 0;
            $numRLANBarrenHomeEU        = 0;

            $numRLANUrbanCorporateAF    = 0;
            $numRLANUrbanPublicAF       = 0;
            $numRLANUrbanHomeAF         = 187875;
            $numRLANSuburbanCorporateAF = 0;
            $numRLANSuburbanPublicAF    = 0;
            $numRLANSuburbanHomeAF      = 101452;
            $numRLANRuralCorporateAF    = 0;
            $numRLANRuralPublicAF       = 0;
            $numRLANRuralHomeAF         = 86422;
            $numRLANBarrenCorporateAF   = 0;
            $numRLANBarrenPublicAF      = 0;
            $numRLANBarrenHomeAF        = 0;

            $numRLANUrbanCorporateME    = 0;
            $numRLANUrbanPublicME       = 0;
            $numRLANUrbanHomeME         = 52945;
            $numRLANSuburbanCorporateME = 0;
            $numRLANSuburbanPublicME    = 0;
            $numRLANSuburbanHomeME      = 28590;
            $numRLANRuralCorporateME    = 0;
            $numRLANRuralPublicME       = 0;
            $numRLANRuralHomeME         = 24355;
            $numRLANBarrenCorporateME   = 0;
            $numRLANBarrenPublicME      = 0;
            $numRLANBarrenHomeME        = 0;
        } elsif ($activeDeviceFlag eq "CEPT_HIGH") {
            $numRLANUrbanCorporateEU    = 0;
            $numRLANUrbanPublicEU       = 0;
            $numRLANUrbanHomeEU         = 1028933;
            $numRLANSuburbanCorporateEU = 0;
            $numRLANSuburbanPublicEU    = 0;
            $numRLANSuburbanHomeEU      = 555624;
            $numRLANRuralCorporateEU    = 0;
            $numRLANRuralPublicEU       = 0;
            $numRLANRuralHomeEU         = 473309;
            $numRLANBarrenCorporateEU   = 0;
            $numRLANBarrenPublicEU      = 0;
            $numRLANBarrenHomeEU        = 0;

            $numRLANUrbanCorporateAF    = 0;
            $numRLANUrbanPublicAF       = 0;
            $numRLANUrbanHomeAF         = 471189;
            $numRLANSuburbanCorporateAF = 0;
            $numRLANSuburbanPublicAF    = 0;
            $numRLANSuburbanHomeAF      = 254442;
            $numRLANRuralCorporateAF    = 0;
            $numRLANRuralPublicAF       = 0;
            $numRLANRuralHomeAF         = 216747;
            $numRLANBarrenCorporateAF   = 0;
            $numRLANBarrenPublicAF      = 0;
            $numRLANBarrenHomeAF        = 0;

            $numRLANUrbanCorporateME    = 0;
            $numRLANUrbanPublicME       = 0;
            $numRLANUrbanHomeME         = 132786;
            $numRLANSuburbanCorporateME = 0;
            $numRLANSuburbanPublicME    = 0;
            $numRLANSuburbanHomeME      = 71704;
            $numRLANRuralCorporateME    = 0;
            $numRLANRuralPublicME       = 0;
            $numRLANRuralHomeME         = 61081;
            $numRLANBarrenCorporateME   = 0;
            $numRLANBarrenPublicME      = 0;
            $numRLANBarrenHomeME        = 0;
        } else {
            die "Invalid activeDeviceFlag = $activeDeviceFlag";
        }

    } elsif (0) {
        if ($activeDeviceFlag eq "CEPT_MID") {
            $numRLANUrbanCorporateEU    = 0;
            $numRLANUrbanPublicEU       = 0;
            $numRLANUrbanHomeEU         = 334273;
            $numRLANSuburbanCorporateEU = 0;
            $numRLANSuburbanPublicEU    = 0;
            $numRLANSuburbanHomeEU      = 180507;
            $numRLANRuralCorporateEU    = 0;
            $numRLANRuralPublicEU       = 0;
            $numRLANRuralHomeEU         = 153765;
            $numRLANBarrenCorporateEU   = 0;
            $numRLANBarrenPublicEU      = 0;
            $numRLANBarrenHomeEU        = 0;

            $numRLANUrbanCorporateAF    = 0;
            $numRLANUrbanPublicAF       = 0;
            $numRLANUrbanHomeAF         = 153077;
            $numRLANSuburbanCorporateAF = 0;
            $numRLANSuburbanPublicAF    = 0;
            $numRLANSuburbanHomeAF      = 82661;
            $numRLANRuralCorporateAF    = 0;
            $numRLANRuralPublicAF       = 0;
            $numRLANRuralHomeAF         = 70415;
            $numRLANBarrenCorporateAF   = 0;
            $numRLANBarrenPublicAF      = 0;
            $numRLANBarrenHomeAF        = 0;

            $numRLANUrbanCorporateME    = 0;
            $numRLANUrbanPublicME       = 0;
            $numRLANUrbanHomeME         = 43138;
            $numRLANSuburbanCorporateME = 0;
            $numRLANSuburbanPublicME    = 0;
            $numRLANSuburbanHomeME      = 23295;
            $numRLANRuralCorporateME    = 0;
            $numRLANRuralPublicME       = 0;
            $numRLANRuralHomeME         = 19844;
            $numRLANBarrenCorporateME   = 0;
            $numRLANBarrenPublicME      = 0;
            $numRLANBarrenHomeME        = 0;
        } elsif ($activeDeviceFlag eq "CEPT_LOW") {
            $numRLANUrbanCorporateEU    = 0;
            $numRLANUrbanPublicEU       = 0;
            $numRLANUrbanHomeEU         = 208254;
            $numRLANSuburbanCorporateEU = 0;
            $numRLANSuburbanPublicEU    = 0;
            $numRLANSuburbanHomeEU      = 112457;
            $numRLANRuralCorporateEU    = 0;
            $numRLANRuralPublicEU       = 0;
            $numRLANRuralHomeEU         = 95797;
            $numRLANBarrenCorporateEU   = 0;
            $numRLANBarrenPublicEU      = 0;
            $numRLANBarrenHomeEU        = 0;

            $numRLANUrbanCorporateAF    = 0;
            $numRLANUrbanPublicAF       = 0;
            $numRLANUrbanHomeAF         = 95368;
            $numRLANSuburbanCorporateAF = 0;
            $numRLANSuburbanPublicAF    = 0;
            $numRLANSuburbanHomeAF      = 51499;
            $numRLANRuralCorporateAF    = 0;
            $numRLANRuralPublicAF       = 0;
            $numRLANRuralHomeAF         = 43869;
            $numRLANBarrenCorporateAF   = 0;
            $numRLANBarrenPublicAF      = 0;
            $numRLANBarrenHomeAF        = 0;

            $numRLANUrbanCorporateME    = 0;
            $numRLANUrbanPublicME       = 0;
            $numRLANUrbanHomeME         = 26876;
            $numRLANSuburbanCorporateME = 0;
            $numRLANSuburbanPublicME    = 0;
            $numRLANSuburbanHomeME      = 14513;
            $numRLANRuralCorporateME    = 0;
            $numRLANRuralPublicME       = 0;
            $numRLANRuralHomeME         = 12363;
            $numRLANBarrenCorporateME   = 0;
            $numRLANBarrenPublicME      = 0;
            $numRLANBarrenHomeME        = 0;
        } elsif ($activeDeviceFlag eq "CEPT_HIGH") {
            $numRLANUrbanCorporateEU    = 0;
            $numRLANUrbanPublicEU       = 0;
            $numRLANUrbanHomeEU         = 583111;
            $numRLANSuburbanCorporateEU = 0;
            $numRLANSuburbanPublicEU    = 0;
            $numRLANSuburbanHomeEU      = 314880;
            $numRLANRuralCorporateEU    = 0;
            $numRLANRuralPublicEU       = 0;
            $numRLANRuralHomeEU         = 268231;
            $numRLANBarrenCorporateEU   = 0;
            $numRLANBarrenPublicEU      = 0;
            $numRLANBarrenHomeEU        = 0;

            $numRLANUrbanCorporateAF    = 0;
            $numRLANUrbanPublicAF       = 0;
            $numRLANUrbanHomeAF         = 267030;
            $numRLANSuburbanCorporateAF = 0;
            $numRLANSuburbanPublicAF    = 0;
            $numRLANSuburbanHomeAF      = 144196;
            $numRLANRuralCorporateAF    = 0;
            $numRLANRuralPublicAF       = 0;
            $numRLANRuralHomeAF         = 122834;
            $numRLANBarrenCorporateAF   = 0;
            $numRLANBarrenPublicAF      = 0;
            $numRLANBarrenHomeAF        = 0;

            $numRLANUrbanCorporateME    = 0;
            $numRLANUrbanPublicME       = 0;
            $numRLANUrbanHomeME         = 75252;
            $numRLANSuburbanCorporateME = 0;
            $numRLANSuburbanPublicME    = 0;
            $numRLANSuburbanHomeME      = 40636;
            $numRLANRuralCorporateME    = 0;
            $numRLANRuralPublicME       = 0;
            $numRLANRuralHomeME         = 34616;
            $numRLANBarrenCorporateME   = 0;
            $numRLANBarrenPublicME      = 0;
            $numRLANBarrenHomeME        = 0;
        } else {
            die "Invalid activeDeviceFlag = $activeDeviceFlag";
        }

    } else {
        if ($activeDeviceFlag eq "CEPT_MID") {
            $numRLANUrbanCorporateEU    = 7495;
            $numRLANUrbanPublicEU       = 1890;
            $numRLANUrbanHomeEU         = 126848;
            $numRLANSuburbanCorporateEU = 2024;
            $numRLANSuburbanPublicEU    = 1021;
            $numRLANSuburbanHomeEU      = 72527;
            $numRLANRuralCorporateEU    = 690;
            $numRLANRuralPublicEU       = 174;
            $numRLANRuralHomeEU         = 66588;
            $numRLANBarrenCorporateEU   = 0;
            $numRLANBarrenPublicEU      = 0;
            $numRLANBarrenHomeEU        = 0;
        } elsif ($activeDeviceFlag eq "CEPT_LOW") {
            $numRLANUrbanCorporateEU    = 5977;
            $numRLANUrbanPublicEU       = 1508;
            $numRLANUrbanHomeEU         = 101155;
            $numRLANSuburbanCorporateEU = 1614;
            $numRLANSuburbanPublicEU    = 814;
            $numRLANSuburbanHomeEU      = 57837;
            $numRLANRuralCorporateEU    = 550;
            $numRLANRuralPublicEU       = 139;
            $numRLANRuralHomeEU         = 53100;
            $numRLANBarrenCorporateEU   = 0;
            $numRLANBarrenPublicEU      = 0;
            $numRLANBarrenHomeEU        = 0;
        } elsif ($activeDeviceFlag eq "CEPT_HIGH") {
            $numRLANUrbanCorporateEU    = 12882;
            $numRLANUrbanPublicEU       = 3249;
            $numRLANUrbanHomeEU         = 218020;
            $numRLANSuburbanCorporateEU = 3478;
            $numRLANSuburbanPublicEU    = 1755;
            $numRLANSuburbanHomeEU      = 124656;
            $numRLANRuralCorporateEU    = 1185;
            $numRLANRuralPublicEU       = 299;
            $numRLANRuralHomeEU         = 114448;
            $numRLANBarrenCorporateEU   = 0;
            $numRLANBarrenPublicEU      = 0;
            $numRLANBarrenHomeEU        = 0;
        } else {
            die "Invalid activeDeviceFlag = $activeDeviceFlag";
        }

        $numRLANUrbanCorporateAF    = 3704;
        $numRLANUrbanPublicAF       = 934;
        $numRLANUrbanHomeAF         = 62688;
        $numRLANSuburbanCorporateAF = 1000;
        $numRLANSuburbanPublicAF    = 505;
        $numRLANSuburbanHomeAF      = 35843;
        $numRLANRuralCorporateAF    = 341;
        $numRLANRuralPublicAF       = 86;
        $numRLANRuralHomeAF         = 32907;
        $numRLANBarrenCorporateAF   = 0;
        $numRLANBarrenPublicAF      = 0;
        $numRLANBarrenHomeAF        = 0;
    }

    $numRLANUrbanCorporate      = "${numRLANUrbanCorporateEU},${numRLANUrbanCorporateAF},${numRLANUrbanCorporateME}";
    $numRLANUrbanPublic         = "${numRLANUrbanPublicEU},${numRLANUrbanPublicAF},${numRLANUrbanPublicME}";
    $numRLANUrbanHome           = "${numRLANUrbanHomeEU},${numRLANUrbanHomeAF},${numRLANUrbanHomeME}";
    $numRLANSuburbanCorporate   = "${numRLANSuburbanCorporateEU},${numRLANSuburbanCorporateAF},${numRLANSuburbanCorporateME}";
    $numRLANSuburbanPublic      = "${numRLANSuburbanPublicEU},${numRLANSuburbanPublicAF},${numRLANSuburbanPublicME}";
    $numRLANSuburbanHome        = "${numRLANSuburbanHomeEU},${numRLANSuburbanHomeAF},${numRLANSuburbanHomeME}";
    $numRLANRuralCorporate      = "${numRLANRuralCorporateEU},${numRLANRuralCorporateAF},${numRLANRuralCorporateME}";
    $numRLANRuralPublic         = "${numRLANRuralPublicEU},${numRLANRuralPublicAF},${numRLANRuralPublicME}";
    $numRLANRuralHome           = "${numRLANRuralHomeEU},${numRLANRuralHomeAF},${numRLANRuralHomeME}";
    $numRLANBarrenCorporate     = "${numRLANBarrenCorporateEU},${numRLANBarrenCorporateAF},${numRLANBarrenCorporateME}";
    $numRLANBarrenPublic        = "${numRLANBarrenPublicEU},${numRLANBarrenPublicAF},${numRLANBarrenPublicME}";
    $numRLANBarrenHome          = "${numRLANBarrenHomeEU},${numRLANBarrenHomeAF},${numRLANBarrenHomeME}";

    $caseList->{"case_${caseIdx}"}->{densityThrUrban}        = 377.3146e-6;
    $caseList->{"case_${caseIdx}"}->{densityThrSuburban}     = 83.6292e-6;
    $caseList->{"case_${caseIdx}"}->{densityThrRural}        = 0.0e-6;

    $indoorFrac = 1.0-$outdoorFrac;
    $thFrac = 0.3;  # 30 % of bldgs thermally efficient for EU (per OFCOM report)

    if ($ulsAntennaPatternFile eq "") {
        $ulsAntennaPatternFile = "ofcom_antennas.csv";
    }
    $caseList->{"case_${caseIdx}"}->{srtmDir}            = "srtm3arcsecondv003";

    $wlanMinFreq = 5935.0e6;
    $wlanMaxFreq = 6415.0e6;
} elsif ($region eq "AC") {
    $caseList->{"case_${caseIdx}"}->{regionStr}  = "AC:3";
    $caseList->{"case_${caseIdx}"}->{popDensityFile}  = "ecc-rlan-study-pop-americas-v1.csv";
    $caseList->{"case_${caseIdx}"}->{popDensityResLon} = 30.0 / 3600;
    $caseList->{"case_${caseIdx}"}->{popDensityResLat} = 30.0 / 3600;
    $caseList->{"case_${caseIdx}"}->{popDensityMinLon} = 20693.0/120;
    $caseList->{"case_${caseIdx}"}->{popDensityNumLon} = 21128;
    $caseList->{"case_${caseIdx}"}->{popDensityMinLat} = -6718.0/120;
    $caseList->{"case_${caseIdx}"}->{popDensityNumLat} = 16755;


    my $numRLANUrbanCorporateAC;
    my $numRLANUrbanPublicAC;
    my $numRLANUrbanHomeAC;
    my $numRLANSuburbanCorporateAC;
    my $numRLANSuburbanPublicAC;
    my $numRLANSuburbanHomeAC;
    my $numRLANRuralCorporateAC;
    my $numRLANRuralPublicAC;
    my $numRLANRuralHomeAC;
    my $numRLANBarrenCorporateAC;
    my $numRLANBarrenPublicAC;
    my $numRLANBarrenHomeAC;

    if (1) {
        if ($activeDeviceFlag eq "CEPT_LOW") {
            $numRLANUrbanCorporateAC    = 0;
            $numRLANUrbanPublicAC       = 0;
            $numRLANUrbanHomeAC         = 143574;
            $numRLANSuburbanCorporateAC = 0;
            $numRLANSuburbanPublicAC    = 0;
            $numRLANSuburbanHomeAC      = 77530;
            $numRLANRuralCorporateAC    = 0;
            $numRLANRuralPublicAC       = 0;
            $numRLANRuralHomeAC         = 66044;
            $numRLANBarrenCorporateAC   = 0;
            $numRLANBarrenPublicAC      = 0;
            $numRLANBarrenHomeAC        = 0;
        } elsif ($activeDeviceFlag eq "CEPT_MID") {
            $numRLANUrbanCorporateAC    = 0;
            $numRLANUrbanPublicAC       = 0;
            $numRLANUrbanHomeAC         = 230453;
            $numRLANSuburbanCorporateAC = 0;
            $numRLANSuburbanPublicAC    = 0;
            $numRLANSuburbanHomeAC      = 124444;
            $numRLANRuralCorporateAC    = 0;
            $numRLANRuralPublicAC       = 0;
            $numRLANRuralHomeAC         = 106008;
            $numRLANBarrenCorporateAC   = 0;
            $numRLANBarrenPublicAC      = 0;
            $numRLANBarrenHomeAC        = 0;
        } elsif ($activeDeviceFlag eq "CEPT_HIGH") {
            $numRLANUrbanCorporateAC    = 0;
            $numRLANUrbanPublicAC       = 0;
            $numRLANUrbanHomeAC         = 360082;
            $numRLANSuburbanCorporateAC = 0;
            $numRLANSuburbanPublicAC    = 0;
            $numRLANSuburbanHomeAC      = 194444;
            $numRLANRuralCorporateAC    = 0;
            $numRLANRuralPublicAC       = 0;
            $numRLANRuralHomeAC         = 165638;
            $numRLANBarrenCorporateAC   = 0;
            $numRLANBarrenPublicAC      = 0;
            $numRLANBarrenHomeAC        = 0;
        } else {
            die "Invalid activeDeviceFlag = $activeDeviceFlag";
        }
    } else {
        if ($activeDeviceFlag eq "CEPT_LOW") {
            $numRLANUrbanCorporateAC    = 0;
            $numRLANUrbanPublicAC       = 0;
            $numRLANUrbanHomeAC         = 72880;
            $numRLANSuburbanCorporateAC = 0;
            $numRLANSuburbanPublicAC    = 0;
            $numRLANSuburbanHomeAC      = 39355;
            $numRLANRuralCorporateAC    = 0;
            $numRLANRuralPublicAC       = 0;
            $numRLANRuralHomeAC         = 33525;
            $numRLANBarrenCorporateAC   = 0;
            $numRLANBarrenPublicAC      = 0;
            $numRLANBarrenHomeAC        = 0;
        } elsif ($activeDeviceFlag eq "CEPT_MID") {
            $numRLANUrbanCorporateAC    = 0;
            $numRLANUrbanPublicAC       = 0;
            $numRLANUrbanHomeAC         = 116981;
            $numRLANSuburbanCorporateAC = 0;
            $numRLANSuburbanPublicAC    = 0;
            $numRLANSuburbanHomeAC      = 63170;
            $numRLANRuralCorporateAC    = 0;
            $numRLANRuralPublicAC       = 0;
            $numRLANRuralHomeAC         = 53811;
            $numRLANBarrenCorporateAC   = 0;
            $numRLANBarrenPublicAC      = 0;
            $numRLANBarrenHomeAC        = 0;
        } elsif ($activeDeviceFlag eq "CEPT_HIGH") {
            $numRLANUrbanCorporateAC    = 0;
            $numRLANUrbanPublicAC       = 0;
            $numRLANUrbanHomeAC         = 204064;
            $numRLANSuburbanCorporateAC = 0;
            $numRLANSuburbanPublicAC    = 0;
            $numRLANSuburbanHomeAC      = 110194;
            $numRLANRuralCorporateAC    = 0;
            $numRLANRuralPublicAC       = 0;
            $numRLANRuralHomeAC         = 93869;
            $numRLANBarrenCorporateAC   = 0;
            $numRLANBarrenPublicAC      = 0;
            $numRLANBarrenHomeAC        = 0;
        } else {
            die "Invalid activeDeviceFlag = $activeDeviceFlag";
        }
    }

    $numRLANUrbanCorporate      = "${numRLANUrbanCorporateAC}";
    $numRLANUrbanPublic         = "${numRLANUrbanPublicAC}";
    $numRLANUrbanHome           = "${numRLANUrbanHomeAC}";
    $numRLANSuburbanCorporate   = "${numRLANSuburbanCorporateAC}";
    $numRLANSuburbanPublic      = "${numRLANSuburbanPublicAC}";
    $numRLANSuburbanHome        = "${numRLANSuburbanHomeAC}";
    $numRLANRuralCorporate      = "${numRLANRuralCorporateAC}";
    $numRLANRuralPublic         = "${numRLANRuralPublicAC}";
    $numRLANRuralHome           = "${numRLANRuralHomeAC}";
    $numRLANBarrenCorporate     = "${numRLANBarrenCorporateAC}";
    $numRLANBarrenPublic        = "${numRLANBarrenPublicAC}";
    $numRLANBarrenHome          = "${numRLANBarrenHomeAC}";

    $caseList->{"case_${caseIdx}"}->{densityThrUrban}        = 377.3146e-6;
    $caseList->{"case_${caseIdx}"}->{densityThrSuburban}     = 83.6292e-6;
    $caseList->{"case_${caseIdx}"}->{densityThrRural}        = 0.0e-6;

    $indoorFrac = 1.0-$outdoorFrac;
    $thFrac = 0.3;  # 30 % of bldgs thermally efficient for EU (per OFCOM report)

    if ($ulsAntennaPatternFile eq "") {
        $ulsAntennaPatternFile = "ofcom_antennas.csv";
    }
    $caseList->{"case_${caseIdx}"}->{srtmDir}            = "srtm3arcsecondv003";

    $wlanMinFreq = 5935.0e6;
    $wlanMaxFreq = 6415.0e6;
} else {
    die "Invalid region";
}

if ($analysisMethod eq "RLAN_TO_GEO") {
    $terrainMinLon = 0.0;
    $terrainMaxLon = -1.0;
    $terrainMinLat = 0.0;
    $terrainMaxLat = -1.0;
} elsif (    ($ulsFile eq "FS_Database_Netherlands.csv")
          || ($ulsFile eq "FS_Database_Netherlands_5.csv")
          || ($ulsFile eq "FS_Database_Netherlands_8.csv")
          || ($ulsFile eq "FS_Database_Netherlands_12.csv")
          || ($ulsFile eq "FS_Database_Netherlands_20.csv")
          || ($ulsFile eq "FS_Database_Netherlands_selecthighint.csv")
          || ($ulsFile eq "FS_Database_Netherlands_selecthighint_180915.csv") ) {
    $terrainMinLon = 1.0;
    $terrainMaxLon = 10.0;
    $terrainMinLat = 48.0;
    $terrainMaxLat = 56.0;
} elsif (    ($ulsFile eq "FS_Database_UK.csv")
          || ($ulsFile eq "FS_Database_UK_532.csv")
          || ($ulsFile eq "FS_Database_UK_533.csv")
          || ($ulsFile eq "FS_Database_UK_531_533.csv")
          || ($ulsFile eq "FS_Database_UK_Top60IovNs.csv")
          || ($ulsFile eq "FS_Database_UK_selecthighint.csv")
          || ($ulsFile eq "FS_Database_UK_selecthighint_180915.csv")
          || ($ulsFile eq "FS_Database_UK_selecthighint_27.csv")
          || ($ulsFile eq "FS_Database_UK_singleuls.csv") ) {
    $terrainMinLon = -10.0;
    $terrainMaxLon = 6.0;
    $terrainMinLat = 46.0;
    $terrainMaxLat = 65.0;
} elsif ($ulsFile eq "FS_Database_French.csv") {
    $terrainMinLon = 0.0;
    $terrainMaxLon = 9.0;
    $terrainMinLat = 40.0;
    $terrainMaxLat = 51.0;
} elsif ($ulsFile eq "output-UL20172211820160-filteredv2.csv") {
    $terrainMinLon = -125.0;
    $terrainMaxLon = -67.0;
    $terrainMinLat = 25.0;
    $terrainMaxLat = 50.0;
} elsif ($ulsFile eq "jacksonville.csv") {
    # Jacksonville, FL
    $terrainMinLon = -81.75;
    $terrainMaxLon = -81.5;
    $terrainMinLat = 30.26;
    $terrainMaxLat = 30.48;
} elsif ($region eq "conus") {
    $terrainMinLon = -125.0;
    $terrainMaxLon = -67.0;
    $terrainMinLat = 25.0;
    $terrainMaxLat = 50.0;
}

$caseList->{"case_${caseIdx}"}->{terrainMinLon} = $terrainMinLon;
$caseList->{"case_${caseIdx}"}->{terrainMaxLon} = $terrainMaxLon;
$caseList->{"case_${caseIdx}"}->{terrainMinLat} = $terrainMinLat;
$caseList->{"case_${caseIdx}"}->{terrainMaxLat} = $terrainMaxLat;

my $thermallyEfficientFrac = $indoorFrac * $thFrac;
my $traditionalFrac = $indoorFrac * (1.0-$thFrac);


$caseList->{"case_${caseIdx}"}->{wlanMinFreq}            = $wlanMinFreq;
$caseList->{"case_${caseIdx}"}->{wlanMaxFreq}            = $wlanMaxFreq;

$caseList->{"case_${caseIdx}"}->{fssStartChanCenterFreq} = 5945.0e6;
$caseList->{"case_${caseIdx}"}->{fssChanSpacing}         =   20.0e6;
$caseList->{"case_${caseIdx}"}->{fssChanBandwidth}       =   36.0e6;
$caseList->{"case_${caseIdx}"}->{fssNumChan}             =   24;

$caseList->{"case_${caseIdx}"}->{fssGonTMethod} = $fssGonTMethod;
$caseList->{"case_${caseIdx}"}->{fssGonTFile} = $fssGonTFile;
$caseList->{"case_${caseIdx}"}->{fssGonTFileOffset} = $fssGonTFileOffset;
$caseList->{"case_${caseIdx}"}->{fssGonT} = $fssGonT;
$caseList->{"case_${caseIdx}"}->{fssGonTMainBeamThr} = $fssGonTMainBeamThr;
$caseList->{"case_${caseIdx}"}->{fssOrbitalSlotStart} = $fssOrbitalSlotStart;
$caseList->{"case_${caseIdx}"}->{fssOrbitalSlotIncr} = $fssOrbitalSlotIncr;
$caseList->{"case_${caseIdx}"}->{fssNumOrbitalSlot} = $fssNumOrbitalSlot;
$caseList->{"case_${caseIdx}"}->{fssPolarizationMismatch} = $fssPolarizationMismatch;
$caseList->{"case_${caseIdx}"}->{fssLinkBudgetFilePfx} = $fssLinkBudgetFilePfx;
$caseList->{"case_${caseIdx}"}->{fssLinkBudgetN} = $fssLinkBudgetN;

$caseList->{"case_${caseIdx}"}->{fsCustomFilterFlag}   = $fsCustomFilterFlag;
$caseList->{"case_${caseIdx}"}->{fsCustomFilterLonMin} = $fsCustomFilterLonMin;
$caseList->{"case_${caseIdx}"}->{fsCustomFilterLonMax} = $fsCustomFilterLonMax;
$caseList->{"case_${caseIdx}"}->{fsCustomFilterLatMin} = $fsCustomFilterLatMin;
$caseList->{"case_${caseIdx}"}->{fsCustomFilterLatMax} = $fsCustomFilterLatMax;

$caseList->{"case_${caseIdx}"}->{ulsDataFile}            = $ulsFile;
$caseList->{"case_${caseIdx}"}->{channelPairingFile}     = $channelPairingFile;
$caseList->{"case_${caseIdx}"}->{ulsNoiseFigureDB}       = $ulsNoiseFigureDB;
$caseList->{"case_${caseIdx}"}->{rlanSensingBandwidth}   = $rlanSensingBandwidth;
$caseList->{"case_${caseIdx}"}->{rlanSensingFrequency}   = $rlanSensingFrequency;
$caseList->{"case_${caseIdx}"}->{rlanNoiseFigureDB}      = 5.0;
$caseList->{"case_${caseIdx}"}->{rlanScaleStr}           = $rlanScaleStr;
$caseList->{"case_${caseIdx}"}->{analysisMethod}         = $analysisMethod;
$caseList->{"case_${caseIdx}"}->{heatmapFull}            = $heatmapFull;
$caseList->{"case_${caseIdx}"}->{heatmapFile}            = $heatmapFile;
$caseList->{"case_${caseIdx}"}->{excThrFile}             = $excThrFile;
$caseList->{"case_${caseIdx}"}->{closeInFile}            = $closeInFile;
$caseList->{"case_${caseIdx}"}->{fresnelFile}            = $fresnelFile;
$caseList->{"case_${caseIdx}"}->{fsHistFile}             = $fsHistFile;
$caseList->{"case_${caseIdx}"}->{oobRadius}              = $oobRadius;
$caseList->{"case_${caseIdx}"}->{fsOOBFile}              = $fsOOBFile;
$caseList->{"case_${caseIdx}"}->{interferenceMitigation} = $interferenceMitigation;
$caseList->{"case_${caseIdx}"}->{interferenceMitigationThreshold} = $interferenceMitigationThreshold;
$caseList->{"case_${caseIdx}"}->{runMitigation} = $runMitigation;
$caseList->{"case_${caseIdx}"}->{mtgnRLANTxPwrDBW} = $mtgnRLANTxPwrDBW;

$caseList->{"case_${caseIdx}"}->{spectrumMapMinFreq}      = $spectrumMapMinFreq;
$caseList->{"case_${caseIdx}"}->{spectrumMapMaxFreq}      = $spectrumMapMaxFreq;
$caseList->{"case_${caseIdx}"}->{spectrumMapThreshold}    = $spectrumMapThreshold;
$caseList->{"case_${caseIdx}"}->{spectrumMapLonStart}     = $spectrumMapLonStart;
$caseList->{"case_${caseIdx}"}->{spectrumMapLonStop}      = $spectrumMapLonStop;
$caseList->{"case_${caseIdx}"}->{spectrumMapLatStart}     = $spectrumMapLatStart;
$caseList->{"case_${caseIdx}"}->{spectrumMapLatStop}      = $spectrumMapLatStop;
$caseList->{"case_${caseIdx}"}->{spectrumMapNumLon}       = $spectrumMapNumLon;
$caseList->{"case_${caseIdx}"}->{spectrumMapNumLat}       = $spectrumMapNumLat;
$caseList->{"case_${caseIdx}"}->{spectrumMapRlanHeight}   = $spectrumMapRlanHeight;
$caseList->{"case_${caseIdx}"}->{spectrumMapRlanEIRP}     = $spectrumMapRlanEIRP;
$caseList->{"case_${caseIdx}"}->{spectrumMapBuildingType} = $spectrumMapBuildingType;

$caseList->{"case_${caseIdx}"}->{visibilityThreshold}     = $visibilityThreshold;
$caseList->{"case_${caseIdx}"}->{fresnelThreshold}        = $fresnelThreshold;
$caseList->{"case_${caseIdx}"}->{removeMobile}            = $removeMobile;

$caseList->{"case_${caseIdx}"}->{ISNumRLAN}               = $ISNumRLAN;
$caseList->{"case_${caseIdx}"}->{ISAngleCoeffDeg}         = $ISAngleCoeffDeg;
$caseList->{"case_${caseIdx}"}->{ISDistCoeffKm}           = $ISDistCoeffKm;

$caseList->{"case_${caseIdx}"}->{ISW2UrbLOSShiftMean}     = $ISW2UrbLOSShiftMean;
$caseList->{"case_${caseIdx}"}->{ISW2UrbNLOSShiftMean}    = $ISW2UrbNLOSShiftMean;
$caseList->{"case_${caseIdx}"}->{ISW2SubLOSShiftMean}     = $ISW2SubLOSShiftMean;
$caseList->{"case_${caseIdx}"}->{ISW2SubNLOSShiftMean}    = $ISW2SubNLOSShiftMean;
$caseList->{"case_${caseIdx}"}->{ISP452Eps}               = $ISP452Eps;

$caseList->{"case_${caseIdx}"}->{ISP2135UrbLOSShiftMean}  = $ISP2135UrbLOSShiftMean;
$caseList->{"case_${caseIdx}"}->{ISP2135UrbNLOSShiftMean} = $ISP2135UrbNLOSShiftMean;
$caseList->{"case_${caseIdx}"}->{ISP2135SubLOSShiftMean}  = $ISP2135SubLOSShiftMean;
$caseList->{"case_${caseIdx}"}->{ISP2135SubNLOSShiftMean} = $ISP2135SubNLOSShiftMean;
$caseList->{"case_${caseIdx}"}->{ISP2135RurLOSShiftMean}  = $ISP2135RurLOSShiftMean;
$caseList->{"case_${caseIdx}"}->{ISP2135RurNLOSShiftMean} = $ISP2135RurNLOSShiftMean;

$caseList->{"case_${caseIdx}"}->{numRLANUrbanCorporate}     =  $numRLANUrbanCorporate;
$caseList->{"case_${caseIdx}"}->{numRLANUrbanPublic}        =  $numRLANUrbanPublic;
$caseList->{"case_${caseIdx}"}->{numRLANUrbanHome}          =  $numRLANUrbanHome;
$caseList->{"case_${caseIdx}"}->{numRLANSuburbanCorporate}  =  $numRLANSuburbanCorporate;
$caseList->{"case_${caseIdx}"}->{numRLANSuburbanPublic}     =  $numRLANSuburbanPublic;
$caseList->{"case_${caseIdx}"}->{numRLANSuburbanHome}       =  $numRLANSuburbanHome;
$caseList->{"case_${caseIdx}"}->{numRLANRuralCorporate}     =  $numRLANRuralCorporate;
$caseList->{"case_${caseIdx}"}->{numRLANRuralPublic}        =  $numRLANRuralPublic;
$caseList->{"case_${caseIdx}"}->{numRLANRuralHome}          =  $numRLANRuralHome;
$caseList->{"case_${caseIdx}"}->{numRLANBarrenCorporate}    =  $numRLANBarrenCorporate;
$caseList->{"case_${caseIdx}"}->{numRLANBarrenPublic}       =  $numRLANBarrenPublic;
$caseList->{"case_${caseIdx}"}->{numRLANBarrenHome}         =  $numRLANBarrenHome;

$caseList->{"case_${caseIdx}"}->{probIndoorBodyLossRLAN}    = $probIndoorBodyLossRLAN;
$caseList->{"case_${caseIdx}"}->{probOutdoorBodyLossRLAN}   = $probOutdoorBodyLossRLAN;

# Building Type Distribution
$caseList->{"case_${caseIdx}"}->{probBtAP}  = "$outdoorFrac:OUTDOOR,$traditionalFrac:TRADITIONAL,$thermallyEfficientFrac:THERMALLY_EFFICIENT";

my $probBwAPIdx = 1;

if ($analysisMethod eq "RLAN_SENSING_MONTE_CARLO") {
    $caseList->{"case_${caseIdx}"}->{probBwAP}  = "1.0:$rlanSensingBandwidth";
} elsif ($probBwAPIdx == 0) {
    # Bandwidth Parameters 2017.08.23
    $caseList->{"case_${caseIdx}"}->{probBwAP}  = "0.05:20.0e6,0.10:40.0e6,0.10:60.0e6,0.35:80.0e6,0.10:100.0e6,0.30:160.0e6";
} elsif ($probBwAPIdx == 1) {
    # Updated Bandwidth Parameters 2017.10.11
    $caseList->{"case_${caseIdx}"}->{probBwAP}  = "0.1:20.0e6,0.1:40.0e6,0.5:80.0e6,0.30:160.0e6";
} else {
    die "ERROR: $!\n";
}

$caseList->{"case_${caseIdx}"}->{clipRLANOutdoorEIRPFlag} = $clipRLANOutdoorEIRPFlag;
$caseList->{"case_${caseIdx}"}->{useEirpPatternFile}      = $useEirpPatternFile;
if ($useEirpPatternFile) {
    $caseList->{"case_${caseIdx}"}->{wifiEirpPatternFile}    = "wifi_eirp_patterns.csv",
    # Antenna Model (outdoor)
    $caseList->{"case_${caseIdx}"}->{probAmAP}  = "0.18:OUTDOOR_CLIENT,0.30:OUTDOOR_DIR_PT_TO_PT,0.10:OUTDOOR_OMNI_PT_TO_MPT,0.42:OUTDOOR_OMNI";

    $caseList->{"case_${caseIdx}"}->{probIndoorEIRPAP}  = "";
    $caseList->{"case_${caseIdx}"}->{probOutdoorEIRPAP} = "";
} else {
    $caseList->{"case_${caseIdx}"}->{wifiEirpPatternFile}    = "",
    # Antenna Model (outdoor)
    $caseList->{"case_${caseIdx}"}->{probAmAP}  = "";

    my @eirpMWList;
    my @indoorPList;
    my @outdoorPList;

    if ($eirpOption eq "baseline") {
        # EIRP distribution updated 20171117
        @eirpMWList   = (4000,   1000,    250,      100,     50,        13,       1);
        @indoorPList  = (0.6692, 0.4199,  10.3870,  6.4858,  24.6441,   51.8367,  5.5573);
        @outdoorPList = (2.8255, 2.0222,  9.4460,   9.0028,  32.1330,   41.9945,  2.5762);
    } elsif ($eirpOption eq "prior_to_20171117") {
        @eirpMWList   = (4000, 1000,   200, 100,  50,   13,   1);
        @indoorPList  = (0.36,  0.9, 16.74,  25,  10,   25,  20);
        @outdoorPList = ( 0.1,  0.6,   0.5, 0.4, 0.2,  0.1, 0.1);
    } elsif ($eirpOption eq "indoor_max_reduced_6") {
        @eirpMWList   = (4000, 1000,   200, 100,  50,   13,   1);
        @indoorPList  = (0.00, 1.26, 16.74,  25,  10,   25,  20);
        @outdoorPList = ( 0.1,  0.6,   0.5, 0.4, 0.2,  0.1, 0.1);
    } elsif ($eirpOption eq "indoor_max_reduced_10") {
        @eirpMWList   = (4000, 1000,   400,   200, 100,  50,   13,   1);
        @indoorPList  = (0.00, 0.00,  1.26, 16.74,  25,  10,   25,  20);
        @outdoorPList = ( 0.1,  0.6,  0.00,   0.5, 0.4, 0.2,  0.1, 0.1);
    } elsif ( ($eirpOption eq "baseline_eu") || ($eirpOption eq "SE45") ) {
        @eirpMWList   = (1000,    250,      100,     50,        13,       1);
        @indoorPList  = (0.7100,  9.1500,   6.2100,  25.7900,   52.4700,  5.6800);
        @outdoorPList = (3.2400,  4.2400,   7.8400,  36.9500,   44.6500,  3.0700);
    } elsif ($eirpOption eq "max") {
        @eirpMWList   = (1000);
        @indoorPList  = (100.0);
        @outdoorPList = (100.0);
    } elsif ($eirpOption eq "const_400mw") {
        @eirpMWList   = (400);
        @indoorPList  = (100.0);
        @outdoorPList = (100.0);
    } else {
        die "ERROR: $!\n";
    }

    my $indoorEirpStr = "";
    my $outdoorEirpStr = "";
    for(my $i=0; $i<int(@eirpMWList); $i++) {
        my $eirpMW = $eirpMWList[$i];
        my $eirpDBW = 10.0*log($eirpMW / 1000)/log(10.0);
        if ($i) {
            $indoorEirpStr .= ",";
            $outdoorEirpStr .= ",";
        }
        $indoorEirpStr  .= "$indoorPList[$i]"  . ":" . "$eirpDBW";
        $outdoorEirpStr .= "$outdoorPList[$i]" . ":" . "$eirpDBW";
    }

    $caseList->{"case_${caseIdx}"}->{probIndoorEIRPAP}  = $indoorEirpStr;
    $caseList->{"case_${caseIdx}"}->{probOutdoorEIRPAP} = $outdoorEirpStr;
}

$caseList->{"case_${caseIdx}"}->{ulsAntennaPatternFile}    = $ulsAntennaPatternFile;
$caseList->{"case_${caseIdx}"}->{probHtUOC} = $outdoorProbHt;
$caseList->{"case_${caseIdx}"}->{probHtUOP} = $outdoorProbHt;
$caseList->{"case_${caseIdx}"}->{probHtUOH} = $outdoorProbHt;
$caseList->{"case_${caseIdx}"}->{probHtUTC} = $indoorUrbanCorporateProbHt;
$caseList->{"case_${caseIdx}"}->{probHtUTP} = $indoorUrbanPublicProbHt;
$caseList->{"case_${caseIdx}"}->{probHtUTH} = $indoorUrbanHomeProbHt;
$caseList->{"case_${caseIdx}"}->{probHtUEC} = $indoorUrbanCorporateProbHt;
$caseList->{"case_${caseIdx}"}->{probHtUEP} = $indoorUrbanPublicProbHt;
$caseList->{"case_${caseIdx}"}->{probHtUEH} = $indoorUrbanHomeProbHt;
$caseList->{"case_${caseIdx}"}->{probHtSOC} = $outdoorProbHt;
$caseList->{"case_${caseIdx}"}->{probHtSOP} = $outdoorProbHt;
$caseList->{"case_${caseIdx}"}->{probHtSOH} = $outdoorProbHt;
$caseList->{"case_${caseIdx}"}->{probHtSTC} = $indoorUrbanCorporateProbHt;
$caseList->{"case_${caseIdx}"}->{probHtSTP} = $indoorUrbanPublicProbHt;
$caseList->{"case_${caseIdx}"}->{probHtSTH} = $indoorSuburbanHomeProbHt;
$caseList->{"case_${caseIdx}"}->{probHtSEC} = $indoorUrbanCorporateProbHt;
$caseList->{"case_${caseIdx}"}->{probHtSEP} = $indoorUrbanPublicProbHt;
$caseList->{"case_${caseIdx}"}->{probHtSEH} = $indoorSuburbanHomeProbHt;
$caseList->{"case_${caseIdx}"}->{probHtROC} = $outdoorProbHt;
$caseList->{"case_${caseIdx}"}->{probHtROP} = $outdoorProbHt;
$caseList->{"case_${caseIdx}"}->{probHtROH} = $outdoorProbHt;
$caseList->{"case_${caseIdx}"}->{probHtRTC} = $indoorRuralProbHt;
$caseList->{"case_${caseIdx}"}->{probHtRTP} = $indoorRuralProbHt;
$caseList->{"case_${caseIdx}"}->{probHtRTH} = $indoorRuralProbHt;
$caseList->{"case_${caseIdx}"}->{probHtREC} = $indoorRuralProbHt;
$caseList->{"case_${caseIdx}"}->{probHtREP} = $indoorRuralProbHt;
$caseList->{"case_${caseIdx}"}->{probHtREH} = $indoorRuralProbHt;
$caseList->{"case_${caseIdx}"}->{probHtBOC} = $outdoorProbHt;
$caseList->{"case_${caseIdx}"}->{probHtBOP} = $outdoorProbHt;
$caseList->{"case_${caseIdx}"}->{probHtBOH} = $outdoorProbHt;
$caseList->{"case_${caseIdx}"}->{probHtBTC} = $indoorRuralProbHt;
$caseList->{"case_${caseIdx}"}->{probHtBTP} = $indoorRuralProbHt;
$caseList->{"case_${caseIdx}"}->{probHtBTH} = $indoorRuralProbHt;
$caseList->{"case_${caseIdx}"}->{probHtBEC} = $indoorRuralProbHt;
$caseList->{"case_${caseIdx}"}->{probHtBEP} = $indoorRuralProbHt;
$caseList->{"case_${caseIdx}"}->{probHtBEH} = $indoorRuralProbHt;

$caseList->{"case_${caseIdx}"}->{maxRadius} = $maxRadius;
$caseList->{"case_${caseIdx}"}->{nearFieldLossFlag} = $nearFieldLossFlag;
$caseList->{"case_${caseIdx}"}->{illuminationEfficiency} = $illuminationEfficiency;
$caseList->{"case_${caseIdx}"}->{noBuildingPenetrationLossFlag} = $noBuildingPenetrationLossFlag;
$caseList->{"case_${caseIdx}"}->{noPolarizationLossFlag} = $noPolarizationLossFlag;
$caseList->{"case_${caseIdx}"}->{exclusionDist} = $exclusionDist;
$caseList->{"case_${caseIdx}"}->{pathLossModel} = $pathLossModel;
$caseList->{"case_${caseIdx}"}->{terrainMethod} = $terrainMethod;
$caseList->{"case_${caseIdx}"}->{clutterFile} = $clutterFile;
$caseList->{"case_${caseIdx}"}->{closeInHgtFlag} = $closeInHgtFlag;
$caseList->{"case_${caseIdx}"}->{closeInHgtLOS} = $closeInHgtLOS;
$caseList->{"case_${caseIdx}"}->{closeInDist} = $closeInDist;
$caseList->{"case_${caseIdx}"}->{closeInPathLossModel} = $closeInPathLossModel;
$caseList->{"case_${caseIdx}"}->{fixedProbFlag} = $fixedProbFlag;
$caseList->{"case_${caseIdx}"}->{compute_pl_20_50_flag}  = $compute_pl_20_50_flag;
$caseList->{"case_${caseIdx}"}->{lidarBeamAttnDB} = $lidarBeamAttnDB;
$caseList->{"case_${caseIdx}"}->{lidarResultFile} = $lidarResultFile;
$caseList->{"case_${caseIdx}"}->{lidarCvgFile} = $lidarCvgFile;
$caseList->{"case_${caseIdx}"}->{lidarDataFileList} = $lidarDataFileList;
$caseList->{"case_${caseIdx}"}->{lidarNumCvgPoints} = $lidarNumCvgPoints;
$caseList->{"case_${caseIdx}"}->{lidarExtractULSFlag} = $lidarExtractULSFlag;
$caseList->{"case_${caseIdx}"}->{lidarExtractULSSelFile} = $lidarExtractULSSelFile;
$caseList->{"case_${caseIdx}"}->{maxLidarRegionLoad} = $maxLidarRegionLoad;
$caseList->{"case_${caseIdx}"}->{lidarWorkingDir} = $lidarWorkingDir;
$caseList->{"case_${caseIdx}"}->{useLidarZIPFiles} = $useLidarZIPFiles;

$caseList->{"case_${caseIdx}"}->{lidarTxEIRPDBW} = $lidarTxEIRPDBW;
$caseList->{"case_${caseIdx}"}->{lidarULSBW} = $lidarULSBW;
$caseList->{"case_${caseIdx}"}->{lidarRLANBW} = $lidarRLANBW;
$caseList->{"case_${caseIdx}"}->{lidarBodyLoss} = $lidarBodyLoss;
$caseList->{"case_${caseIdx}"}->{lidarBldgLossDB} = $lidarBldgLossDB;
$caseList->{"case_${caseIdx}"}->{lidarPathClutterLossDB} = $lidarPathClutterLossDB;
$caseList->{"case_${caseIdx}"}->{lidarPolarizationLossDB} = $lidarPolarizationLossDB;
$caseList->{"case_${caseIdx}"}->{lidarCvgConeOnly} = $lidarCvgConeOnly;
$caseList->{"case_${caseIdx}"}->{lidarMinOcclusionHt} = $lidarMinOcclusionHt;
$caseList->{"case_${caseIdx}"}->{lidarMinOcclusionDist} = $lidarMinOcclusionDist;
$caseList->{"case_${caseIdx}"}->{lidarMaxOcclusionDist} = $lidarMaxOcclusionDist;
$caseList->{"case_${caseIdx}"}->{lidarTxOcclusionRadius} = $lidarTxOcclusionRadius;
$caseList->{"case_${caseIdx}"}->{lidarBlacklistFile} = $lidarBlacklistFile;

$caseList->{"case_${caseIdx}"}->{filterSimRegionOnly}    = $filterSimRegionOnly;

$caseList->{"case_${caseIdx}"}->{ulsPositionOption} = $ulsPositionOption;
$caseList->{"case_${caseIdx}"}->{ulsCenterLongitudeDeg} = $ulsCenterLongitudeDeg;
$caseList->{"case_${caseIdx}"}->{ulsCenterLatitudeDeg} = $ulsCenterLatitudeDeg;
$caseList->{"case_${caseIdx}"}->{ulsRadius} = $ulsRadius;
$caseList->{"case_${caseIdx}"}->{numULS} = $numULS;
$caseList->{"case_${caseIdx}"}->{adjustSimulationRegion} = $adjustSimulationRegion;
$caseList->{"case_${caseIdx}"}->{useMultiThreading} = $useMultiThreading;

$caseList->{"case_${caseIdx}"}->{cityList} = \@cityList;

$caseList->{"case_${caseIdx}"}->{runType} = $runType;
$caseList->{"case_${caseIdx}"}->{runFlag} = $runFlag;
$caseIdx++;
}


$| = 1;

my $helpmsg =
    "Script run_rlanafc.pl options:\n"
  . "    -run_sim                Run rlanafc simulation cases\n"
  . "    -no_query               Don't prompt for comfirmation before running\n"
  . "    -i                      Run in interactive mode prompting before each command is executed\n"
  . "    -h                      Print this help message\n"
  . "\n";

while ($_ = $ARGV[0]) {
    shift;
    if (/^-run_sim/) {
        $run_sim_flag = 1;
    } elsif (/^-no_query/) {
        $query = 0;
    } elsif (/^-i/) {
        $interactive = 1;
    } elsif (/^-h/) {
        print $helpmsg;
        exit;
    } else {
        print "ERROR: Invalid command line options\n";
        exit;
    }
}

if ( ($run_sim_flag == 0) ) {
    print "No action specified.  Use \"run_rlanafc.pl -run_sim\" to run simulation cases.\n";
    exit;
}

$caseIdx = 0;
my $runIdx = 0;
print "The following cases will be run:\n";
while ($caseList->{"case_${caseIdx}"}) {

    my $case = $caseList->{"case_${caseIdx}"};

    if ($case->{runFlag}) {
        print "    ($runIdx) " . $case->{name} . " [${caseIdx}]"
            . "\n";
        $runIdx++;
    }

    $caseIdx++;
}
if ($query) {
    my $resp;
    print "Are you sure you want to continue (y/n)? ";
    chop($resp = <STDIN>);
    if ($resp ne "y") {
        exit;
    }
    print "\n";
}


$caseIdx = 0;
while ($caseList->{"case_${caseIdx}"}) {

    my $case = $caseList->{"case_${caseIdx}"};

    print "RUN CASE = $case->{runFlag}" . "\n";

    if ($case->{runFlag}) {
        runCase($case);
    }

    $caseIdx++;
}

exit;

#################################################################################
# runCase
#################################################################################
sub runCase
{
    my $case = $_[0];
    my $numThread = 4;
    my $firstLine;
    my $lastLine;
    my $flagFile;

    my $case_dir = "${sim_dir_pfx}/$case->{dir}";
    mkpath($case_dir, 1, 0755);
    execute_command("chmod 755 $case_dir", $interactive);

    if ($case->{runType} eq "singlePoint") {
        my $templFile = "/tmp/templ_rlanafc_$$.txt";
        my $name = $case->{name};
        my $logfile = "data_${name}.txt";

        writeTemplate($case, $templFile);

        my $cmd = "./rlanafc ulsInterferenceAnalysis --templ=$templFile >| $logfile";
        execute_command("$cmd", $interactive);
    } elsif ( $case->{runType} eq "extract_uls_citylist") {
        my @cityList = @{$caseList->{"case_${caseIdx}"}->{cityList}};
        my $cityIdx;
        my $namePfx = $case->{name};
        for($cityIdx=0; $cityIdx<int(@cityList); $cityIdx++) {
            my $cityStr = $cityList[$cityIdx];
            $case->{lidarDataFileList} = $cityStr;
            my @clist = split("/", $cityStr);
            my $cityName = pop @clist;

            print "CITY = $cityName\n";
            $case->{lidarExtractULSSelFile} = "select_lines_${cityName}.txt";

            my $templFile = "/tmp/templ_rlanafc_$$.txt";
            my $name = $namePfx . "_" . $cityName;

            $case->{name} = $name;
            my $logfile = "data_${name}.txt";

            writeTemplate($case, $templFile);

            my $cmd = "./rlanafc ulsInterferenceAnalysis --templ=$templFile >| $logfile";
            execute_command("$cmd", $interactive);

            $cmd = "select_lines.pl $case->{ulsDataFile} $case->{lidarExtractULSSelFile} >| ulsfile_${cityName}.csv";
            execute_command("$cmd", $interactive);
        }
    } elsif ( $case->{runType} eq "lidar_citylist") {
        my @cityList = @{$caseList->{"case_${caseIdx}"}->{cityList}};
        my $cityIdx;
        my $namePfx = $case->{name};
        for($cityIdx=0; $cityIdx<int(@cityList); $cityIdx++) {
            my $cityStr = (split(":", $cityList[$cityIdx], -1))[0];
            my $lidarBlacklistFile = (split(":", $cityList[$cityIdx], -1))[1];

            $case->{lidarDataFileList} = $cityStr;
            $case->{lidarBlacklistFile} = $lidarBlacklistFile;

            my @clist = split("/", $cityStr);
            my $cityName = pop @clist;

            print "CITY = $cityName\n";

            my $templFile = "/tmp/templ_rlanafc_$$.txt";
            my $name = $namePfx . "_" . $cityName;

            $case->{name} = $name;
            $case->{ulsDataFile} = "ulsfile_${cityName}.csv";
            $case->{lidarResultFile} = "${name}_lidar_result.csv";
            $case->{lidarCvgFile} = "${name}_lidar_cvg.txt";
            my $logfile = "data_${name}.txt";

            writeTemplate($case, $templFile);

            my $cmd = "./rlanafc ulsInterferenceAnalysis --templ=$templFile >| $logfile";
            execute_command("$cmd", $interactive);
        }
    }

    return;
}
#################################################################################

#################################################################################
# Routine writeTemplate
#################################################################################
sub writeTemplate
{
    my $case = $_[0];
    my $filename = $_[1];
    my $param_val;
    my $numParam;
    my $s;

    $s = "";
    my $paramIdx = 0;

    $s .= "PARAM_${paramIdx}: NAME \"$case->{name}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: NUM_TRIAL $case->{numTrial}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: REGION_STR \"$case->{regionStr}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: POP_DENSITY_FILE \"$case->{popDensityFile}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: POP_DENSITY_RES_LON $case->{popDensityResLon}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: POP_DENSITY_RES_LAT $case->{popDensityResLat}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: POP_DENSITY_MIN_LON $case->{popDensityMinLon}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: POP_DENSITY_NUM_LON $case->{popDensityNumLon}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: POP_DENSITY_MIN_LAT $case->{popDensityMinLat}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: POP_DENSITY_NUM_LAT $case->{popDensityNumLat}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: TERRAIN_METHOD \"$case->{terrainMethod}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: SRTM_DIR \"$case->{srtmDir}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: TERRAIN_MIN_LON $case->{terrainMinLon}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: TERRAIN_MAX_LON $case->{terrainMaxLon}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: TERRAIN_MIN_LAT $case->{terrainMinLat}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: TERRAIN_MAX_LAT $case->{terrainMaxLat}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: DENSITY_THR_URBAN $case->{densityThrUrban}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: DENSITY_THR_SUBURBAN $case->{densityThrSuburban}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: DENSITY_THR_RURAL $case->{densityThrRural}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: ANALYSIS_METHOD \"$case->{analysisMethod}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: WLAN_MIN_FREQ $case->{wlanMinFreq}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: WLAN_MAX_FREQ $case->{wlanMaxFreq}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: FSS_START_CHAN_CENTER_FREQ $case->{fssStartChanCenterFreq}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: FSS_CHAN_SPACING $case->{fssChanSpacing}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: FSS_CHAN_BANDWIDTH $case->{fssChanBandwidth}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: FSS_NUM_CHAN $case->{fssNumChan}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: FSS_G_ON_T_METHOD \"$case->{fssGonTMethod}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: FSS_G_ON_T_FILE \"$case->{fssGonTFile}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: FSS_G_ON_T_FILE_OFFSET $case->{fssGonTFileOffset}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: FSS_G_ON_T $case->{fssGonT}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: FSS_G_ON_T_MAIN_BEAM_THR $case->{fssGonTMainBeamThr}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: FSS_ORBITAL_SLOT_START $case->{fssOrbitalSlotStart}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: FSS_ORBITAL_SLOT_INCR $case->{fssOrbitalSlotIncr}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: FSS_ORBITAL_SLOT_INCR $case->{fssOrbitalSlotIncr}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: FSS_NUM_ORBITAL_SLOT $case->{fssNumOrbitalSlot}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: FSS_POLARIZATION_MISMATCH $case->{fssPolarizationMismatch}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: FSS_LINK_BUDGET_FILE_PFX \"$case->{fssLinkBudgetFilePfx}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: FSS_LINK_BUDGET_N $case->{fssLinkBudgetN}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: FS_CUSTOM_FILTER_FLAG $case->{fsCustomFilterFlag}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: FS_CUSTOM_FILTER_LON_MIN $case->{fsCustomFilterLonMin}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: FS_CUSTOM_FILTER_LON_MAX $case->{fsCustomFilterLonMax}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: FS_CUSTOM_FILTER_LAT_MIN $case->{fsCustomFilterLatMin}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: FS_CUSTOM_FILTER_LAT_MAX $case->{fsCustomFilterLatMax}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: ULS_DATA_FILE \"$case->{ulsDataFile}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: CHANNEL_PAIRING_FILE \"$case->{channelPairingFile}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: ULS_NOISE_FIGURE_DB $case->{ulsNoiseFigureDB}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: RLAN_SENSING_BANDWIDTH $case->{rlanSensingBandwidth}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: RLAN_SENSING_FREQUENCY $case->{rlanSensingFrequency}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: RLAN_NOISE_FIGURE_DB $case->{rlanNoiseFigureDB}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: RLAN_SCALE_STR \"$case->{rlanScaleStr}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: RUN_MITIGATION $case->{runMitigation}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: MTGN_RLAN_TX_PWR_DBW $case->{mtgnRLANTxPwrDBW}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: USE_EIRP_PATTERN_FILE $case->{useEirpPatternFile}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: CLIP_RLAN_OUTDOOR_EIRP_FLAG $case->{clipRLANOutdoorEIRPFlag}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: RLAN_EIRP_PATTERN_FILE \"$case->{wifiEirpPatternFile}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: ULS_ANTENNA_PATTERN_FILE \"$case->{ulsAntennaPatternFile}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: PROB_AM_AP \"$case->{probAmAP}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: PROB_INDOOR_EIRP_AP \"$case->{probIndoorEIRPAP}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: PROB_OUTDOOR_EIRP_AP \"$case->{probOutdoorEIRPAP}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: PROB_INDOOR_BODY_LOSS_RLAN \"$case->{probIndoorBodyLossRLAN}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: PROB_OUTDOOR_BODY_LOSS_RLAN \"$case->{probOutdoorBodyLossRLAN}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: HEATMAP_FULL $case->{heatmapFull}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: HEATMAP_FILE \"$case->{heatmapFile}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: EXC_THR_FILE \"$case->{excThrFile}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: CLOSE_IN_FILE \"$case->{closeInFile}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: FRESNEL_FILE \"$case->{fresnelFile}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: FS_HIST_FILE \"$case->{fsHistFile}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: OOB_RADIUS $case->{oobRadius}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: FS_OOB_FILE \"$case->{fsOOBFile}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: NUM_AP_URBAN_CORPORATE \"$case->{numRLANUrbanCorporate}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: NUM_AP_URBAN_PUBLIC \"$case->{numRLANUrbanPublic}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: NUM_AP_URBAN_HOME \"$case->{numRLANUrbanHome}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: NUM_AP_SUBURBAN_CORPORATE \"$case->{numRLANSuburbanCorporate}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: NUM_AP_SUBURBAN_PUBLIC \"$case->{numRLANSuburbanPublic}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: NUM_AP_SUBURBAN_HOME \"$case->{numRLANSuburbanHome}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: NUM_AP_RURAL_CORPORATE \"$case->{numRLANRuralCorporate}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: NUM_AP_RURAL_PUBLIC \"$case->{numRLANRuralPublic}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: NUM_AP_RURAL_HOME \"$case->{numRLANRuralHome}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: NUM_AP_BARREN_CORPORATE \"$case->{numRLANBarrenCorporate}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: NUM_AP_BARREN_PUBLIC \"$case->{numRLANBarrenPublic}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: NUM_AP_BARREN_HOME \"$case->{numRLANBarrenHome}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: PROB_BW_AP \"$case->{probBwAP}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: PROB_BT_AP \"$case->{probBtAP}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: PROB_HT_URBAN_OUTDOOR_CORPORATE \"$case->{probHtUOC}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_URBAN_OUTDOOR_PUBLIC \"$case->{probHtUOP}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_URBAN_OUTDOOR_HOME \"$case->{probHtUOH}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_URBAN_TRADITIONAL_CORPORATE \"$case->{probHtUTC}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_URBAN_TRADITIONAL_PUBLIC \"$case->{probHtUTP}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_URBAN_TRADITIONAL_HOME \"$case->{probHtUTH}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_URBAN_THERMALLY_EFFICIENT_CORPORATE \"$case->{probHtUEC}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_URBAN_THERMALLY_EFFICIENT_PUBLIC \"$case->{probHtUEP}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_URBAN_THERMALLY_EFFICIENT_HOME \"$case->{probHtUEH}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_SUBURBAN_OUTDOOR_CORPORATE \"$case->{probHtSOC}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_SUBURBAN_OUTDOOR_PUBLIC \"$case->{probHtSOP}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_SUBURBAN_OUTDOOR_HOME \"$case->{probHtSOH}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_SUBURBAN_TRADITIONAL_CORPORATE \"$case->{probHtSTC}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_SUBURBAN_TRADITIONAL_PUBLIC \"$case->{probHtSTP}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_SUBURBAN_TRADITIONAL_HOME \"$case->{probHtSTH}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_SUBURBAN_THERMALLY_EFFICIENT_CORPORATE \"$case->{probHtSEC}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_SUBURBAN_THERMALLY_EFFICIENT_PUBLIC \"$case->{probHtSEP}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_SUBURBAN_THERMALLY_EFFICIENT_HOME \"$case->{probHtSEH}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_RURAL_OUTDOOR_CORPORATE \"$case->{probHtROC}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_RURAL_OUTDOOR_PUBLIC \"$case->{probHtROP}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_RURAL_OUTDOOR_HOME \"$case->{probHtROH}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_RURAL_TRADITIONAL_CORPORATE \"$case->{probHtRTC}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_RURAL_TRADITIONAL_PUBLIC \"$case->{probHtRTP}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_RURAL_TRADITIONAL_HOME \"$case->{probHtRTH}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_RURAL_THERMALLY_EFFICIENT_CORPORATE \"$case->{probHtREC}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_RURAL_THERMALLY_EFFICIENT_PUBLIC \"$case->{probHtREP}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_RURAL_THERMALLY_EFFICIENT_HOME \"$case->{probHtREH}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_BARREN_OUTDOOR_CORPORATE \"$case->{probHtBOC}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_BARREN_OUTDOOR_PUBLIC \"$case->{probHtBOP}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_BARREN_OUTDOOR_HOME \"$case->{probHtBOH}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_BARREN_TRADITIONAL_CORPORATE \"$case->{probHtBTC}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_BARREN_TRADITIONAL_PUBLIC \"$case->{probHtBTP}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_BARREN_TRADITIONAL_HOME \"$case->{probHtBTH}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_BARREN_THERMALLY_EFFICIENT_CORPORATE \"$case->{probHtBEC}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_BARREN_THERMALLY_EFFICIENT_PUBLIC \"$case->{probHtBEP}\"\n";
    $paramIdx++;
    $s .= "PARAM_${paramIdx}: PROB_HT_BARREN_THERMALLY_EFFICIENT_HOME \"$case->{probHtBEH}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: MAX_RADIUS $case->{maxRadius}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: NEAR_FIELD_LOSS_FLAG $case->{nearFieldLossFlag}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: ILLUMINATION_EFFICIENCY $case->{illuminationEfficiency}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: NO_BUILDING_PENETRATION_LOSS_FLAG $case->{noBuildingPenetrationLossFlag}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: NO_POLARIZATION_LOSS_FLAG $case->{noPolarizationLossFlag}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: EXCLUSION_DIST $case->{exclusionDist}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: PATH_LOSS_MODEL \"$case->{pathLossModel}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: CLUTTER_FILE \"$case->{clutterFile}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: FIXED_PROB_FLAG $case->{fixedProbFlag}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: COMPUTE_PL_20_50_FLAG $case->{compute_pl_20_50_flag}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: ULS_POSITION_OPTION \"$case->{ulsPositionOption}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: ULS_CENTER_LONGITUDE_DEG $case->{ulsCenterLongitudeDeg}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: ULS_CENTER_LATITUDE_DEG $case->{ulsCenterLatitudeDeg}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: ULS_RADIUS $case->{ulsRadius}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: NUM_ULS $case->{numULS}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: ADJUST_SIMULATION_REGION $case->{adjustSimulationRegion}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: USE_MULTI_THREADING $case->{useMultiThreading}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: CLOSE_IN_HGT_FLAG $case->{closeInHgtFlag}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: CLOSE_IN_HGT_LOS $case->{closeInHgtLOS}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: CLOSE_IN_DIST $case->{closeInDist}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: CLOSE_IN_PATH_LOSS_MODEL \"$case->{closeInPathLossModel}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: LIDAR_BEAM_ATTN_DB $case->{lidarBeamAttnDB}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: LIDAR_RESULT_FILE \"$case->{lidarResultFile}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: LIDAR_CVG_FILE \"$case->{lidarCvgFile}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: LIDAR_DATA_FILE \"$case->{lidarDataFileList}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: LIDAR_NUM_CVG_POINTS $case->{lidarNumCvgPoints}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: LIDAR_EXTRACT_ULS_FLAG $case->{lidarExtractULSFlag}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: LIDAR_EXTRACT_ULS_SEL_FILE \"$case->{lidarExtractULSSelFile}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: MAX_LIDAR_REGION_LOAD $case->{maxLidarRegionLoad}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: LIDAR_WORKING_DIR \"$case->{lidarWorkingDir}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: USE_LIDAR_ZIP_FILES $case->{useLidarZIPFiles}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: LIDAR_TX_EIRP_DBW $case->{lidarTxEIRPDBW}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: LIDAR_ULS_BW $case->{lidarULSBW}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: LIDAR_RLAN_BW $case->{lidarRLANBW}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: LIDAR_BODY_LOSS $case->{lidarBodyLoss}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: LIDAR_BLDG_LOSS $case->{lidarBldgLossDB}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: LIDAR_PATH_CLUTTER_LOSS $case->{lidarPathClutterLossDB}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: LIDAR_POLARIZATION_LOSS $case->{lidarPolarizationLossDB}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: LIDAR_CVG_CONE_ONLY $case->{lidarCvgConeOnly}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: LIDAR_MIN_OCCLUSION_HT $case->{lidarMinOcclusionHt}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: LIDAR_MIN_OCCLUSION_DIST $case->{lidarMinOcclusionDist}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: LIDAR_MAX_OCCLUSION_DIST $case->{lidarMaxOcclusionDist}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: LIDAR_TX_OCCLUSION_RADIUS $case->{lidarTxOcclusionRadius}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: LIDAR_BLACKLIST_FILE \"$case->{lidarBlacklistFile}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: INTERFERENCE_MITIGATION $case->{interferenceMitigation}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: INTERFERENCE_MITIGATION_THRESHOLD $case->{interferenceMitigationThreshold}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: SPECTRUM_MAP_MIN_FREQ $case->{spectrumMapMinFreq}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: SPECTRUM_MAP_MAX_FREQ $case->{spectrumMapMaxFreq}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: SPECTRUM_MAP_THRESHOLD $case->{spectrumMapThreshold}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: SPECTRUM_MAP_LON_START $case->{spectrumMapLonStart}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: SPECTRUM_MAP_LON_STOP $case->{spectrumMapLonStop}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: SPECTRUM_MAP_LAT_START $case->{spectrumMapLatStart}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: SPECTRUM_MAP_LAT_STOP $case->{spectrumMapLatStop}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: SPECTRUM_MAP_NUM_LON $case->{spectrumMapNumLon}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: SPECTRUM_MAP_NUM_LAT $case->{spectrumMapNumLat}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: SPECTRUM_MAP_RLAN_EIRP $case->{spectrumMapRlanEIRP}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: SPECTRUM_MAP_BUILDING_TYPE \"$case->{spectrumMapBuildingType}\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: VISIBILITY_THRESHOLD $case->{visibilityThreshold}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: FRESNEL_THRESHOLD $case->{fresnelThreshold}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: REMOVE_MOBILE $case->{removeMobile}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: IS_NUM_RLAN $case->{ISNumRLAN}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: IS_ANGLE_COEFF_DEG $case->{ISAngleCoeffDeg}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: IS_DIST_COEFF_KM $case->{ISDistCoeffKm}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: IS_W2_URBLOS_SHIFT_MEAN $case->{ISW2UrbLOSShiftMean}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: IS_W2_URBNLOS_SHIFT_MEAN $case->{ISW2UrbNLOSShiftMean}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: IS_W2_SUBLOS_SHIFT_MEAN $case->{ISW2SubLOSShiftMean}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: IS_W2_SUBNLOS_SHIFT_MEAN $case->{ISW2SubNLOSShiftMean}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: IS_P452_EPS $case->{ISP452Eps}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: IS_P2135_URBLOS_SHIFT_MEAN $case->{ISP2135UrbLOSShiftMean}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: IS_P2135_URBNLOS_SHIFT_MEAN $case->{ISP2135UrbNLOSShiftMean}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: IS_P2135_SUBLOS_SHIFT_MEAN $case->{ISP2135SubLOSShiftMean}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: IS_P2135_SUBNLOS_SHIFT_MEAN $case->{ISP2135SubNLOSShiftMean}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: IS_P2135_RURLOS_SHIFT_MEAN $case->{ISP2135RurLOSShiftMean}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: IS_P2135_RURNLOS_SHIFT_MEAN $case->{ISP2135RurNLOSShiftMean}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: FILTER_SIM_REGION_ONLY $case->{filterSimRegionOnly}\n";
    $paramIdx++;

    $numParam = $paramIdx;

    open(FILE, ">$filename") || die "Can't open file $filename: $!\n";
    print FILE "# COALITION 6GHZ INTERFERENCE SIMULATION TEMPLATE FILE\n";
    print FILE "FORMAT: 1_0\n";
    print FILE "\n";
    print FILE "NUM_PARAM: ${numParam}\n";
    print FILE "\n";
    print FILE "$s";
    close FILE;

    return;
}
#################################################################################

#################################################################################
#################################################################################
#################################################################################

#################################################################################
# Routine execute_command:
#################################################################################
sub execute_command
{
    my $cmd         = $_[0];
    my $interactive = $_[1];

    my $exec_flag = 1;

    if ($interactive) {
        my $resp;
        print "Are you sure you want to execute command: $cmd (y/n)? ";
        chop($resp = <STDIN>);
        if ($resp ne "y") {
            $exec_flag = 0;
        }
    }
    if ($exec_flag) {
        print "$cmd\n";
        system($cmd);
        if ($? == -1) {
            die "failed to execute: $!\n";
        } elsif ($? & 127) {
            my $errmsg = sprintf("child died with signal %d, %s coredump\n", ($? & 127),  ($? & 128) ? 'with' : 'without');
            die "$errmsg : $!\n";
        }
    }

    return;
}
#################################################################################

