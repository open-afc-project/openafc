#!/usr/bin/perl -w
################################################################################
#### FILE: create_lidar_database.pl                                         ####
#### Take list of cities for which src lidar data has been downloaded.      ####
#### This data consists of:                                                 ####
####     (1) Bare Earth raster files                                        ####
####     (2) 3D building shape files                                        ####
####     (3) 2D building shape files (only when 3D files missing)           ####
#### For each city:                                                         ####
####     (1) Identity bare earth and building files available.              ####
####     (2) Identify pairs of files where a pair consists of a bare earth  ####
####         and building polygon file that cover the same region.          ####
####     (3) Convert bare earth into tif raster file, and convert building  ####
####         polygon file into tif raster file where both files are on same ####
####         lon/lat grid.                                                  ####
####     (4) Combine bare earth and building tif files into a single tif    ####
####         file with bare earth on Band 1 and building height on band 2.  ####
####     (5) Under target directory create directory for each city, under   ####
####         each city create dir structure containing combined tif files.  ####
####     (6) For each city create info file listing tif files and min/max   ####
####         lon/lat for each file.                                         ####
#### Optionally create kml file:                                            ####
####     (1) Src lidar data that has been downloaded.                       ####
####     (2) Rectangular bounding box for each file.                        ####
####     (3) Coverage region for shape files.                               ####
####     (4) Identify regions with no data in raster files.                 ####
################################################################################

use File::Path;
use List::Util;
use Cwd;
# use POSIX;
use strict;
use threads;
use threads::shared;
use IO::Handle;

my $pi = 2*atan2(1.0, 0.0);
my $earthRadius = 6378.137e3;

my $interactive = 0;
my $query       = 1;
my $cleantmp    = 1;
my $lonlatRes = "0.00001";
my $gpMaxPixel = 32768;
my $nodataVal  = 1.0e30;
my $imageLonLatRes = 0.001;  # For kml image of vector coverage
my $heightFieldName = "topelev_m";
my $processLidarFlag = 1;
my $srcKMLFlag    = 1;
my $inclKMLBoundingBoxes = 0;

my $sourceDir;
my $destDir;
my $tmpDir;
my $name;

my $dataset;
# $dataset = "2019";
# $dataset = "old";
$dataset = "test";

if ($dataset eq "2019") {
    $sourceDir = "/run/media/dknopoff/rat_data/lidar_2019/rockyftp.cr.usgs.gov/vdelivery/Datasets/Staged/Elevation/Non_Standard_Contributed/NGA_US_Cities";
    $destDir   = "/run/media/dknopoff/rat_data/proc_lidar_2019";
    $tmpDir   = "/run/media/dknopoff/rat_data/create_lidar_database_tmp";
    $name = "NGA_US_Cities";
} elsif ($dataset eq "old") {
    $sourceDir = "/run/media/mmandell/rkf_data/terrain/lidar/lidar_old";
    $destDir   = "/run/media/mmandell/rkf_data/terrain/lidar/proc_lidar_old";
    $tmpDir   = "/data/tmp/tmp/create_lidar_database_tmp";
    $name = "old_lidar";
} elsif ($dataset eq "test") {
    $sourceDir = "/run/media/dknopoff/rkf_data/lidar_2019/rockyftp.cr.usgs.gov/vdelivery/Datasets/Staged/Elevation/Non_Standard_Contributed/NGA_US_Cities";
    $destDir   = "/run/media/dknopoff/rkf_data/temp_out";
    $tmpDir   = "/run/media/dknopoff/rkf_data/temp";
    $name = "NGA_US_Cities";
} else {
    die "ERROR: dataset set to illegal value: $dataset : $!\n";
}

my $kmlTmp = "${tmpDir}/kml";
my $logTmp = "${tmpDir}/log";
my $srcKML = "${destDir}/src_${name}.kmz";

my $defaultRasterClampMin =  -100.0;
my $defaultRasterClampMax =  5000.0;
my $defaultRasterMinMag   =     0.0;

my @cityList;

#####################################################################
# Declare shared variables, set numThread
my @unprocessedCityList:shared;
my %loghash:shared;
my %kmlhash:shared;
my %discardhash:shared;

my $numThread = 2;
#####################################################################
my $cityStartIdx = 0; # For debugging

if ($dataset eq "old") {
    @cityList = (
        "${sourceDir}/Washington_DC",

        ""
    );
} elsif ($dataset eq "test") {
    @cityList = (
        # "${sourceDir}/Norfolk_VA",
        # "${sourceDir}/Lancaster_PA",
        # "${sourceDir}/Winston-Salem_NC:100:350:0.0",
        # "${sourceDir}/Atlanta_GA",
        # "${sourceDir}/San_Francisco_CA",
        # "${sourceDir}/Jackson_MS",
        # "${sourceDir}/Augusta_ME",
        # "${sourceDir}/Cleveland_OH",
        # "${sourceDir}/Chattanooga_TN",
        "${sourceDir}/Norfolk_VA",
        "${sourceDir}/Providence_RI",
        "${sourceDir}/Cincinnati_OH",
        "${sourceDir}/New_Orleans_LA",
        "${sourceDir}/Chicago_IL",
        "${sourceDir}/Tampa_FL",
        "${sourceDir}/Grand_Rapids_MI",
        "${sourceDir}/New_York_NY",
        "${sourceDir}/Baton_Rouge_LA",
        "${sourceDir}/Los_Angeles_CA",
        "${sourceDir}/Charlotte_NC",
        "${sourceDir}/Denver_CO",
        ""
    );
} elsif ($dataset eq "2019") {
# $cityStartIdx = 93;
@cityList = (

    # The selected 10 cities
    "${sourceDir}/New_York_NY",

    "${sourceDir}/Atlanta_GA",
    "${sourceDir}/Boston_MA",
    "${sourceDir}/Chicago_IL",
    "${sourceDir}/Dallas_TX",
    "${sourceDir}/Houston_TX",
    "${sourceDir}/Los_Angeles_CA",
    "${sourceDir}/Miami_FL",
    "${sourceDir}/Philadelphia_PA",
    "${sourceDir}/San_Francisco_CA",

    "${sourceDir}/Albany_NY",
    "${sourceDir}/Albuquerque_NM",
    "${sourceDir}/Allentown_PA",
    "${sourceDir}/Amarillo_TX",
    "${sourceDir}/Anaheim_CA",
    "${sourceDir}/Anchorage_AK",
    "${sourceDir}/Augusta_GA",
    "${sourceDir}/Augusta_ME",
    "${sourceDir}/Austin_TX",
    "${sourceDir}/Bakersfield_CA",
    "${sourceDir}/Baltimore_MD",
    "${sourceDir}/Barre_Montpelier_VT",
    "${sourceDir}/Baton_Rouge_LA",
    "${sourceDir}/Birmingham_AL",
    "${sourceDir}/Bismarck_ND",
    "${sourceDir}/Boise_ID",
    "${sourceDir}/Bridgeport_CT",
    "${sourceDir}/Buffalo_NY",
    "${sourceDir}/Carson_City_NV",
    "${sourceDir}/Charleston_SC",
    "${sourceDir}/Charleston_WV",
    "${sourceDir}/Charlotte_NC",
    "${sourceDir}/Chattanooga_TN",
    "${sourceDir}/Cheyenne_WY",
    "${sourceDir}/Cincinnati_OH",
    "${sourceDir}/Cleveland_OH",
    "${sourceDir}/Colorado_Springs_CO",
    "${sourceDir}/Columbia_SC",
    "${sourceDir}/Columbus_GA",
    "${sourceDir}/Columbus_OH",
    "${sourceDir}/Concord_NH",
    "${sourceDir}/Corpus_Christi_TX",
    "${sourceDir}/Dayton_OH",
    "${sourceDir}/Denver_CO",
    "${sourceDir}/DesMoines_IA",
    "${sourceDir}/Detroit_MI",
    "${sourceDir}/Dover_DE",
    "${sourceDir}/El_Paso_TX",
    "${sourceDir}/Flint_MI",
    "${sourceDir}/Fort_Wayne_IN",
    "${sourceDir}/Frankfort_KY",
    "${sourceDir}/Fresno_CA",
    "${sourceDir}/Grand_Rapids_MI",
    "${sourceDir}/Greensboro_NC",
    "${sourceDir}/Harrisburg_PA",
    "${sourceDir}/Hartford_CT",
    "${sourceDir}/Helena_MT",
    "${sourceDir}/Honolulu_HI",
    "${sourceDir}/Huntsville_AL",
    "${sourceDir}/Indianapolis_IN",
    "${sourceDir}/Jackson_MS",
    "${sourceDir}/Jacksonville_FL",
    "${sourceDir}/Jefferson_City_MO",
    "${sourceDir}/Juneau_AK",
    "${sourceDir}/Kansas_City_MO",
    "${sourceDir}/Knoxville_TN",
    "${sourceDir}/Lancaster_PA",
    "${sourceDir}/Lansing_MI",
    "${sourceDir}/Las_Vegas_NV",
    "${sourceDir}/Lexington_KY",
    "${sourceDir}/Lincoln_NE",
    "${sourceDir}/Little_Rock_AR",
    "${sourceDir}/Louisville_KY",
    "${sourceDir}/Lubbock_TX",
    "${sourceDir}/Madison_WI",
    "${sourceDir}/McAllen_TX",
    "${sourceDir}/Memphis_TN",
    "${sourceDir}/Milwaukee_WI",
    "${sourceDir}/Minneapolis_MN",
    "${sourceDir}/Mission_Viejo_CA",
    "${sourceDir}/Mobile_AL",
    "${sourceDir}/Modesto_CA",
    "${sourceDir}/Montgomery_AL",
    "${sourceDir}/Nashville_TN",
    "${sourceDir}/New_Haven_CT",
    "${sourceDir}/New_Orleans_LA",
    "${sourceDir}/Norfolk_VA",
    "${sourceDir}/Oklahoma_City_OK",
    "${sourceDir}/Olympia_WA",
    "${sourceDir}/Omaha_NE",
    "${sourceDir}/Orlando_FL",
    "${sourceDir}/Oxnard_CA",
    "${sourceDir}/Palm_Bay_FL",
    "${sourceDir}/Pensacola_FL",
    "${sourceDir}/Phoenix_AZ",
    "${sourceDir}/Pierre_SD",
    "${sourceDir}/Pittsburgh_PA",
    "${sourceDir}/Portland_OR",
    "${sourceDir}/Poughkeepsie_NY",
    "${sourceDir}/Providence_RI",
    "${sourceDir}/Raleigh-Durham_NC:$defaultRasterClampMin:$defaultRasterClampMax:0.00001",
    "${sourceDir}/Reno_NV",
    "${sourceDir}/Richmond_VA",
    "${sourceDir}/Riverside-San_Bernardino_CA",
    "${sourceDir}/Rochester_NY",
    "${sourceDir}/Sacramento_CA",
    "${sourceDir}/Salem_OR",
    "${sourceDir}/Salt_Lake_City_UT",
    "${sourceDir}/San_Antonio_TX",
    "${sourceDir}/San_Diego_CA",
    "${sourceDir}/San_Jaun_PR",
    "${sourceDir}/Santa_Fe_NM",
    "${sourceDir}/Sarasota_FL",
    "${sourceDir}/Scranton_PA",
    "${sourceDir}/Seattle_WA",
    "${sourceDir}/Shreveport_LA",
    "${sourceDir}/Spokane_WA",
    "${sourceDir}/Springfield_IL",
    "${sourceDir}/Springfield_MA",
    "${sourceDir}/St_Louis_MO",
    "${sourceDir}/Stockton_CA",
    "${sourceDir}/Syracuse_NY",
    "${sourceDir}/Tallahassee_FL",
    "${sourceDir}/Tampa_FL",
    "${sourceDir}/Toledo_OH",
    "${sourceDir}/Topeka_KS",
    "${sourceDir}/Trenton_NJ",
    "${sourceDir}/Tucson_AZ",
    "${sourceDir}/Tulsa_OK",
    "${sourceDir}/Vancouver_BC",
    "${sourceDir}/Whistler_Mountain_BC",
    "${sourceDir}/Wichita_KS",
    "${sourceDir}/Winston-Salem_NC:100:350:0.0",
    "${sourceDir}/Worcester_MA",
    "${sourceDir}/Youngstown_OH",

    ""
);
} else {
    die "ERROR: dataset set to illegal value: $dataset : $!\n";
}
pop(@cityList);

splice @cityList,0,$cityStartIdx;

@unprocessedCityList = @cityList;

$| = 1;

my $discardedFiles;

my $helpmsg =
    "Script create_lidar_database.pl options:\n"
  . "    -no_query               Don't prompt for comfirmation before running\n"
  . "    -no_process_lidar       Don't actually create processed lidar files\n"
  . "    -no_source_kml          Don't create source kml file\n"
  . "    -no_cleantmp            Don't remove files from tmp, useful for debugging\n"
  . "    -i                      Run in interactive mode prompting before each command is executed\n"
  . "    -h                      Print this help message\n"
  . "\n";

while ($_ = $ARGV[0]) {
    shift;
    if (/^-no_query/) {
        $query = 0;
    } elsif (/^-no_cleantmp/) {
        $cleantmp = 0;
    } elsif (/^-no_process_lidar/) {
        $processLidarFlag = 0;
    } elsif (/^-no_source_kml/) {
        $srcKMLFlag = 0;
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

my $cityIdx;
print "Processing Lidar Data for " . int(@cityList) . " cities:\n";
for ($cityIdx=0; $cityIdx<int(@cityList); $cityIdx++) {
    my $city = $cityList[$cityIdx];
    my $cityName = substr $city, rindex( $city, '/' ) + length '/';
    print "    ($cityIdx) $cityName\n";
}
print "PROCESS LIDAR: $processLidarFlag\n";
print "DESTINATION DIRECTORY: $destDir\n";
print "CREATE SOURCE KML: $srcKMLFlag\n";
if ($srcKMLFlag) { print "SRC KML FILE: $srcKML\n"; }

if ($query) {
    my $resp;
    print "Are you sure you want to continue (y/n)? ";
    chop($resp = <STDIN>);
    if ($resp ne "y") {
        exit;
    }
    print "\n";
}

if (!(-e $destDir)) {
    die "ERROR: Destination directory: $destDir does not exist : $!\n";
}

if (-e $tmpDir) {
    print "Removing Tmp Directory \"$tmpDir\" ... \n";
    rmtree(${tmpDir});
}

if (-e $tmpDir) {
    die "ERROR removing Tmp Directory \"$tmpDir\"\n";
} else {
    print "Tmp Directory \"$tmpDir\" does not exist, creating ... \n";
    mkpath($tmpDir, 0, 0755);
    if (-e $tmpDir) {
        print "Tmp Directory \"$tmpDir\" successfully created.\n";
    } else {
        die "ERROR: Unable to create directory: $tmpDir : $!\n";
    }
}

if ($srcKMLFlag) {
    mkpath("${kmlTmp}", 0, 0755);
    if (!(-e "${kmlTmp}")) {
        die "ERROR: Unable to create directory: ${kmlTmp} : $!\n";
    }
}

mkpath("${logTmp}", 0, 0755);
if (!(-e "${logTmp}")) {
    die "ERROR: Unable to create directory: ${logTmp} : $!\n";
}

my $tstart = time;

my $thrIdx;
my @thrList;
for($thrIdx=0; $thrIdx<$numThread; $thrIdx++) {
    $thrList[$thrIdx] = threads->create(\&runThread);
}

for($thrIdx=0; $thrIdx<$numThread; $thrIdx++) {
    $thrList[$thrIdx]->join();
}

#############################################################################
# Print log information 
#############################################################################
if (1) {
    my $city;
    my $discardStr = "";
    foreach $city (sort @cityList) {
        if (exists($loghash{$city})) {
            print $loghash{$city};
        }
        $discardStr .= $discardhash{$city};
    }

    my @discardList = split("\n", $discardStr);
    print "TOTAL NUMBER OF DISCARDED FILES: " . int(@discardList) . "\n";

    for(my $dIdx=0; $dIdx<@discardList; $dIdx++) {
        print "($dIdx) $discardList[$dIdx]\n";
    }
}
#############################################################################

#############################################################################
# Create source KML file
#############################################################################
if ($srcKMLFlag) {
    my $kmlFile = "${kmlTmp}/doc.kml";
    open(SRCKMLFILE, ">${kmlFile}") || die "Can't open file $kmlFile : $!\n";
    print SRCKMLFILE genKML("HEAD");
    my $city;
    foreach $city (sort @cityList) {
        if (exists($kmlhash{$city})) {
            print SRCKMLFILE $kmlhash{$city};
        }
    }
    print SRCKMLFILE genKML("TAIL");

    close SRCKMLFILE;

    my $cwd = cwd();
    chdir($kmlTmp);

    my $cmd = "zip -r $srcKML .";
    print execute_command($cmd, $interactive);

    chdir($cwd);
}
#############################################################################

my $tend = time;

print "TOTAL RUNNING TIME = " . ($tend-$tstart) . " sec\n";

exit;

#################################################################################
# runThread    
#################################################################################
sub runThread
{
    my $city;
    my $cont = 1;
    do {
        {
            lock(@unprocessedCityList);
            if (int(@unprocessedCityList) == 0) {
                $cont = 0;
            } else {
                $city = shift(@unprocessedCityList);
            }
        }

        if ($cont) {
            processCity($city);
        }
    } while($cont);
}
#################################################################################

#################################################################################
# processCity
#################################################################################
sub processCity
{
    my $cityStr = $_[0];

    my $kml = "";
    my $discard = "";

    my @clist = split(":", $cityStr);

    my $city;
    my $fixRasterParams = {};
    if (int(@clist) == 1) {
        $city = $clist[0];
        $fixRasterParams->{clampMin} = $defaultRasterClampMin;
        $fixRasterParams->{clampMax} = $defaultRasterClampMax;
        $fixRasterParams->{minMag}   = $defaultRasterMinMag;
    } elsif (int(@clist) == 4) {
        $city = $clist[0];
        $fixRasterParams->{clampMin} = $clist[1];
        $fixRasterParams->{clampMax} = $clist[2];
        $fixRasterParams->{minMag}   = $clist[3];
    } else {
        die "ERROR: Invalid cityStr = \"$cityStr\" : $!\n";
    }

    my $cityName = substr $city, rindex( $city, '/' ) + length '/';
    my $topDir = substr $city, 0, rindex( $city, '/' );
    my $infoFile = "${destDir}/${cityName}_info.csv";
    my $cmd;
    my $t0 = time;

    print "Beginning: $cityName ...\n";

    my $logFile = "${logTmp}/${cityName}_log.txt";
    open(LOGFILE, ">${logFile}") || die "Can't open file $logFile : $!\n";
    LOGFILE->autoflush(1);

    print LOGFILE "Processing City: $cityName\n";

    if ($processLidarFlag) {
        if (-e "${destDir}/${cityName}") {
            die "ERROR: Destination directory ${destDir}/${cityName} already exists : $!\n";
        }

        #############################################################################
        # Open INFO file and write header
        #############################################################################
        open(INFOFILE, ">$infoFile") || die "Can't open file $infoFile : $!\n";

        print INFOFILE "FILE,MIN_LON_DEG,MAX_LON_DEG,MIN_LAT_DEG,MAX_LAT_DEG\n";
        #############################################################################
    }

    my @currPath = ();

    #############################################################################
    # Determine list of subdirectories under city
    #############################################################################
    my @dirlist = ($cityName);
    my @subdirlist;

    while(@dirlist) {
        my $dir = pop @dirlist;
        my $beDir = "${topDir}/${dir}/BE";
        my $bldgDir = "${topDir}/${dir}/VEC/BLD3";
        my $bldg2Dir = "${topDir}/${dir}/VEC/BLD2";
        if ((-e $beDir) && ((-e $bldgDir) || (-e $bldg2Dir)) ) {
            if ((-d $beDir) && ((-d $bldgDir) || (-d $bldg2Dir)) ) {
                push @subdirlist, $dir;
            }
        }

        opendir DIR, "${topDir}/$dir";
        my @filelist = grep !/^\.\.?$/, readdir DIR;
        closedir DIR;
        my $filename;
        foreach $filename (@filelist) {
            if (-d "${topDir}/${dir}/$filename") {
                push @dirlist, "$dir/$filename";
            }
        }
    }

    # Sort subdirlist in reverse order so that when subdir contains a year, newest subdirs appear before older ones.
    @subdirlist = sort {lc($b) cmp lc($a)} @subdirlist;
    
    print LOGFILE "Found ". (int(@subdirlist)) . " subdirectories: \n";
    my $sdIdx;
    for($sdIdx=0; $sdIdx<int(@subdirlist); $sdIdx++) {
        my $sd = $subdirlist[$sdIdx];
        print LOGFILE "    ($sdIdx) $sd\n";
    }
    #############################################################################

    #############################################################################
    # Process subdirectories one at a time
    #############################################################################
    my $cityRegion = {};
    for($sdIdx=0; $sdIdx<int(@subdirlist); $sdIdx++) {
        my $sd = $subdirlist[$sdIdx];
        my $srcBEDir    = "${topDir}/${sd}/BE";
        my $srcBldgDir  = "${topDir}/${sd}/VEC/BLD3";
        my $srcBldg2Dir = "${topDir}/${sd}/VEC/BLD2";

        my $relSD = "${sd}" . "/";
        $relSD =~ s/^$cityName\///;

        #########################################################################
        # Create list of SRC bare earth files
        #########################################################################
        opendir DIR, $srcBEDir;
        my @srcBElist = grep /\.(img|tif)$/, readdir DIR;
        closedir DIR;
        #########################################################################

        #########################################################################
        # Fix each SRC bare earth file by removing invalid values and properly
        # setting nodata values.  Also create image file showing coverage.
        # Note that for raster files, files are FIXED prior to determining extents
        # because fixing the file may result in no valid data in the file in which 
        # case it is discarded.
        #########################################################################
        my $beDir = "${tmpDir}/fixed/${sd}/BE";
        my @belist = ();
        if (!(-e "${beDir}")) {
            mkpath("${beDir}", 0, 0755);
            if (!(-e "${beDir}")) {
                die "ERROR: Unable to create directory: ${beDir} : $!\n";
            }
        } else {
            die "ERROR: directory: ${beDir} already exists : $!\n";
        }
        my $srcBEIdx;
        for($srcBEIdx=0; $srcBEIdx<int(@srcBElist); $srcBEIdx++) {
            my $srcBEFile = $srcBEDir . "/" . $srcBElist[$srcBEIdx];

            my $beBaseFile;
            if ($srcBElist[$srcBEIdx] =~ /^(.*)\.img$/) {
                $beBaseFile = $1;
            } elsif ($srcBElist[$srcBEIdx] =~ /^(.*)\.tif$/) {
                $beBaseFile = $1;
            } else {
                die "ERROR: Invalid BE filename: $srcBElist[$srcBEIdx] : $!\n";
            }

            my $fixedBEFile    = $beDir . "/" . $beBaseFile . ".tif";
            my $imageBEFile    = $beDir . "/" . $beBaseFile . ".png";
            my $tmpImageBEFile = $beDir . "/" . $beBaseFile . ".ppm";

            my $resp = fixRaster($srcBEFile, $fixedBEFile, $imageBEFile, $tmpImageBEFile, $fixRasterParams, $cityName);
            print LOGFILE $resp;

            my $numData;
            if ($resp =~ /.*Num DATA values: (\d+) /m) {
                $numData = $1;
            } else {
                print LOGFILE "ERROR: Unable to extract num DATA values from fixRaster command\n";
                $numData = 0;
            }

            if ($numData) {
                push @belist, $beBaseFile . ".tif";
            } else {
                my $discFile = "${sd}/BE/" . $srcBElist[$srcBEIdx];
                print LOGFILE "Bare earth file: $discFile contains no data, DISCARDED\n";
                $discard .= "$discFile : contains no data\n";
            }
        }
        #########################################################################

        #########################################################################
        # For each fixed BE file, determine the extents of the file
        #########################################################################
        my $beIdx = 0;
        my $beFile;
        my $beData = {};
        while($beIdx<int(@belist)) {
            $beFile = $beDir . "/" . $belist[$beIdx];
            my $beInfo = `gdalinfo -norat -nomd -noct $beFile`;

            my $ulLon;
            my $ulLat;
            my $llLon;
            my $llLat;
            my $urLon;
            my $urLat;
            my $lrLon;
            my $lrLat;
            if ($beInfo =~ /.*Upper Left[^\n]*\(([^\n()]*)\)\nLower Left[^\n]*\(([^\n()]*)\)\nUpper Right[^\n]*\(([^\n()]*)\)\nLower Right[^\n]*\(([^\n()]*)\)\n/m) {
                my $ulStr = $1;
                my $llStr = $2;
                my $urStr = $3;
                my $lrStr = $4;

                # print "UL: $ulStr\n";
                # print "LL: $llStr\n";
                # print "UR: $urStr\n";
                # print "LR: $lrStr\n";

                ($ulLon, $ulLat) = extractLonLat($ulStr);
                ($llLon, $llLat) = extractLonLat($llStr);
                ($urLon, $urLat) = extractLonLat($urStr);
                ($lrLon, $lrLat) = extractLonLat($lrStr);

                my $minLon = List::Util::max($ulLon, $llLon);
                my $maxLon = List::Util::min($urLon, $lrLon);

                my $minLat = List::Util::max($llLat, $lrLat);
                my $maxLat = List::Util::min($ulLat, $urLat);

                $beData->{"region_${beIdx}"}->{minLon} = $minLon;
                $beData->{"region_${beIdx}"}->{maxLon} = $maxLon;
                $beData->{"region_${beIdx}"}->{minLat} = $minLat;
                $beData->{"region_${beIdx}"}->{maxLat} = $maxLat;
                # print "$minLon, $minLat\n";
                # print "$minLon, $maxLat\n";
                # print "$maxLon, $maxLat\n";
                # print "$maxLon, $minLat\n";
                # print "$minLon, $minLat\n";
                # print "\n"
                $beIdx++;
            } else {
                my $discFile = "${sd}/BE/" . $belist[$beIdx];
                print LOGFILE "Unable to extract coverage area for bare earth file: $discFile, DISCARDED\n";
                $discard .= "$discFile : Unable to extract coverage area for bare earth file\n";

                splice @belist, $beIdx, 1;
            }
        }
        #########################################################################

        #########################################################################
        # Create list of SRC 3D Bldg files
        #########################################################################
        my @srcBldglist = ();
        if (-d $srcBldgDir) {
            opendir DIR, $srcBldgDir;
            @srcBldglist = grep /\.shp$/, readdir DIR;
            closedir DIR;
        }
        #########################################################################

        #########################################################################
        # For each SRC 3D Bldg file, get coverage area, run fixVector
        # to replace MultiPolygon's with Polygons and count NUM_POLYGON
        #########################################################################
        my $bldgDir = "${tmpDir}/fixed/${sd}/BLD3";
        my @bldglist = ();
        if (!(-e "${bldgDir}")) {
            mkpath("${bldgDir}", 0, 0755);
            if (!(-e "${bldgDir}")) {
                die "ERROR: Unable to create directory: ${bldgDir} : $!\n";
            }
        } else {
            die "ERROR: directory: ${bldgDir} already exists : $!\n";
        }
        my $bldgData = {};
        my $srcBldgIdx;
        my $bldgIdx = 0;
        for($srcBldgIdx=0; $srcBldgIdx<int(@srcBldglist); $srcBldgIdx++) {
            my $srcBldgFile = $srcBldgDir . "/" . $srcBldglist[$srcBldgIdx];
            my $layerName = substr $srcBldglist[$srcBldgIdx], 0, -4;

            my $bldgInfo = `ogrinfo -so -nomd $srcBldgFile $layerName`;

            my $discardFlag = 0;
            my $discardMsg;

            my $minLon;
            my $minLat;
            my $maxLon;
            my $maxLat;
            if ($bldgInfo =~ /.*Extent:\s*\(([-\d.]+),\s*([-\d.]+)\)\s*-\s*\(([-\d.]+),\s*([-\d.]+)\)\n/m) {
                $minLon = $1;
                $minLat = $2;
                $maxLon = $3;
                $maxLat = $4;
                if (    ($minLon == 0.0) && ($maxLon == 0.0)
                     && ($minLat == 0.0) && ($maxLat == 0.0) ) {
                    $discardFlag = 1;
                    my $discFile = "${sd}/VEC/BLD3/" . $srcBldglist[$srcBldgIdx];
                    $discardMsg = "$discFile : file extent is zero\n";
                } else {
                    # print "$minLon, $minLat\n";
                    # print "$minLon, $maxLat\n";
                    # print "$maxLon, $maxLat\n";
                    # print "$maxLon, $minLat\n";
                    # print "$minLon, $minLat\n";
                    # print "\n";
                }
            } else {
                $discardFlag = 1;
                my $discFile = "${sd}/VEC/BLD3/" . $srcBldglist[$srcBldgIdx];
                $discardMsg = "$discFile : Unable to extract coverage area for 3D BLDG file\n";
            }

            my $hgtFieldName;
            if (!$discardFlag) {
                if ($bldgInfo =~ /.*topelev_m: /m) {
                    $hgtFieldName = "topelev_m";
                } elsif ($bldgInfo =    ~ /.*TOPELEV_M: /m) {
                    $hgtFieldName = "TOPELEV_M";
                } elsif ($bldgInfo =~ /.*TopElev3d: /m) {
                    $hgtFieldName = "TopElev3d";
                } elsif ($bldgInfo =~ /.*TopElev3D: /m) {
                    $hgtFieldName = "TopElev3D";
                } elsif ($bldgInfo =~ /.*TOPELEV3D: /m) {
                    $hgtFieldName = "TOPELEV3D";
                } else {
                    $discardFlag = 1;
                    my $discFile = "${sd}/VEC/BLD3/" . $srcBldglist[$srcBldgIdx];
                    $discardMsg = "$discFile : Unable to determine field name for elevation in 3D-Bldg file\n";
                }
            }

            my $bldgBaseFile;
            if (!$discardFlag) {
                if ($srcBldglist[$srcBldgIdx] =~ /^(.*)\.shp$/) {
                    $bldgBaseFile = $1;
                } else {
                    die "ERROR: Invalid Bldg filename: $srcBldglist[$srcBldgIdx] : $!\n";
                }

                # Sqlite has problems when filenames contain '-', replace with underscore '_'
                $bldgBaseFile =~ s/\-/_/g;

                my $fixedBldgFile = $bldgDir . "/" . $bldgBaseFile . ".shp";
                my $imageBldgFile = $bldgDir . "/" . $bldgBaseFile . ".png";
                my $tmpImageBldgFile = $bldgDir . "/" . $bldgBaseFile . ".ppm";

                my $resp = fixVector($srcBldgFile, $hgtFieldName, $fixedBldgFile, $imageBldgFile, $tmpImageBldgFile, $cityName);

                print LOGFILE $resp;

                my $numPolygon;
                if ($resp =~ /.*NUM_POLYGON: (\d+)/m) {
                    $numPolygon = $1;
                } else {
                    print LOGFILE "ERROR: Unable to extract NUM_POLYGON value from fixVector command\n";
                    $numPolygon = 0;
                }

                if ($numPolygon == 0) {
                    $discardFlag = 1;
                    my $discFile = "${sd}/VEC/BLD3/" . $bldglist[$bldgIdx];
                    $discardMsg = "$discFile : contains no polygons\n";
                }
            }

            if (!$discardFlag) {
                push @bldglist, $bldgBaseFile . ".shp";
                $bldgData->{"region_${bldgIdx}"}->{minLon} = $minLon;
                $bldgData->{"region_${bldgIdx}"}->{maxLon} = $maxLon;
                $bldgData->{"region_${bldgIdx}"}->{minLat} = $minLat;
                $bldgData->{"region_${bldgIdx}"}->{maxLat} = $maxLat;
                $bldgIdx++;
            } else {
                print LOGFILE "DISCARDED: $discardMsg";
                $discard .= $discardMsg;
            }
        }
        #########################################################################

        #########################################################################
        # Create list of SRC 2D Bldg files
        #########################################################################
        my @srcBldg2list = ();
        if (-d $srcBldg2Dir) {
            opendir DIR, $srcBldg2Dir;
            @srcBldg2list = grep /\.shp$/, readdir DIR;
            closedir DIR;
        }
        #########################################################################

        #########################################################################
        # For each SRC 2D Bldg file, get coverage area, run fixVector
        # to replace MultiPolygon's with Polygons and count NUM_POLYGON
        #########################################################################
        my $bldg2Dir = "${tmpDir}/fixed/${sd}/BLD2";
        my @bldg2list = ();
        if (!(-e "${bldg2Dir}")) {
            mkpath("${bldg2Dir}", 0, 0755);
            if (!(-e "${bldg2Dir}")) {
                die "ERROR: Unable to create directory: ${bldg2Dir} : $!\n";
            }
        } else {
            die "ERROR: directory: ${bldg2Dir} already exists : $!\n";
        }
        my $bldg2Data = {};
        my $srcBldg2Idx = {};
        my $bldg2Idx = 0;
        for($srcBldg2Idx=0; $srcBldg2Idx<int(@srcBldg2list); $srcBldg2Idx++) {
            my $srcBldg2File = $srcBldg2Dir . "/" . $srcBldg2list[$srcBldg2Idx];
            my $layerName = substr $srcBldg2list[$srcBldg2Idx], 0, -4;

            my $bldg2Info = `ogrinfo -so -nomd $srcBldg2File $layerName`;

            my $discardFlag = 0;
            my $discardMsg;

            my $minLon;
            my $minLat;
            my $maxLon;
            my $maxLat;
            if ($bldg2Info =~ /.*Extent:\s*\(([-\d.]+),\s*([-\d.]+)\)\s*-\s*\(([-\d.]+),\s*([-\d.]+)\)\n/m) {
                $minLon = $1;
                $minLat = $2;
                $maxLon = $3;
                $maxLat = $4;

                if (    ($minLon == 0.0) && ($maxLon == 0.0)
                     && ($minLat == 0.0) && ($maxLat == 0.0) ) {
                    $discardFlag = 1;
                    my $discFile = "${sd}/VEC/BLD2/" . $srcBldg2list[$srcBldg2Idx];
                    $discardMsg =  "$discFile : file extent is zero\n";
                } else {
                    # print "$minLon, $minLat\n";
                    # print "$minLon, $maxLat\n";
                    # print "$maxLon, $maxLat\n";
                    # print "$maxLon, $minLat\n";
                    # print "$minLon, $minLat\n";
                    # print "\n";
                }
            } else {
                $discardFlag = 1;
                my $discFile = "${sd}/VEC/BLD2/" . $srcBldg2list[$srcBldg2Idx];
                $discardMsg = "$discFile : Unable to extract coverage area for 2D BLDG file\n";
            }

            my $hgtFieldName;
            if (!$discardFlag) {
                if ($bldg2Info =~ /.*maxht_m: /m) {
                    $hgtFieldName = "maxht_m";
                } elsif ($bldg2Info =~ /.*MAXHT_M: /m) {
                    $hgtFieldName = "MAXHT_M";
                } elsif ($bldg2Info =~ /.*HGT2d: /m) {
                    $hgtFieldName = "HGT2d";
                } else {
                    $discardFlag = 1;
                    my $discFile = "${sd}/VEC/BLD2/" . $srcBldg2list[$srcBldg2Idx];
                    $discardMsg = "$discFile : Unable to determine field name for elevation in 2D-Bldg file\n";
                }
            }

            my $bldgBaseFile;
            if (!$discardFlag) {
                if ($srcBldg2list[$srcBldg2Idx] =~ /^(.*)\.shp$/) {
                    $bldgBaseFile = $1;
                } else {
                    die "ERROR: Invalid Bldg filename: $srcBldg2list[$srcBldg2Idx] : $!\n";
                }

                # Sqlite has problems when filenames contain '-', replace with underscore '_'
                $bldgBaseFile =~ s/\-/_/g;

                my $fixedBldgFile = $bldg2Dir . "/" . $bldgBaseFile . ".shp";
                my $imageBldgFile = $bldg2Dir . "/" . $bldgBaseFile . ".png";
                my $tmpImageBldgFile = $bldg2Dir . "/" . $bldgBaseFile . ".ppm";

                my $resp = fixVector($srcBldg2File, $hgtFieldName, $fixedBldgFile, $imageBldgFile, $tmpImageBldgFile, $cityName);

                print LOGFILE $resp;

                my $numPolygon;
                if ($resp =~ /.*NUM_POLYGON: (\d+)/m) {
                    $numPolygon = $1;
                } else {
                    print LOGFILE "ERROR: Unable to extract NUM_POLYGON value from fixVector command\n";
                    $numPolygon = 0;
                }

                if ($numPolygon == 0) {
                    $discardFlag = 1;
                    my $discFile = "${sd}/VEC/BLD2/" . $bldg2list[$bldg2Idx];
                    print LOGFILE "2D building file: $discFile contains no polygons, DISCARDED\n";
                    $discardMsg = "$discFile : contains no polygons\n";
                }
            }

            if (!$discardFlag) {
                push @bldg2list, $bldgBaseFile . ".shp";
                $bldg2Data->{"region_${bldg2Idx}"}->{minLon} = $minLon;
                $bldg2Data->{"region_${bldg2Idx}"}->{maxLon} = $maxLon;
                $bldg2Data->{"region_${bldg2Idx}"}->{minLat} = $minLat;
                $bldg2Data->{"region_${bldg2Idx}"}->{maxLat} = $maxLat;
                $bldg2Idx++;
            } else {
                print LOGFILE "DISCARDED: $discardMsg";
                $discard .= $discardMsg;
            }
        }
        #########################################################################
        my $beRegion = {};
        for($beIdx=0; $beIdx<int(@belist); $beIdx++) {
            $beRegion = regionUnion($beRegion, $beData->{"region_${beIdx}"});
        }
        my $bldgRegion = {};
        for($bldgIdx=0; $bldgIdx<int(@bldglist); $bldgIdx++) {
            $bldgRegion = regionUnion($bldgRegion, $bldgData->{"region_${bldgIdx}"});
        }
        my $bldg2Region = {};
        for($bldg2Idx=0; $bldg2Idx<int(@bldg2list); $bldg2Idx++) {
            $bldg2Region = regionUnion($bldg2Region, $bldg2Data->{"region_${bldg2Idx}"});
        }

        my $sdRegion = {};
        $sdRegion = regionUnion($sdRegion, $beRegion);
        $sdRegion = regionUnion($sdRegion, $bldgRegion);
        $sdRegion = regionUnion($sdRegion, $bldg2Region);

        $cityRegion = regionUnion($cityRegion, $sdRegion);

        if ($srcKMLFlag) {
            my @path = split("/", $sd);

            my $i;
            my $currIdx;
            my $found = 0;
            for($i=0; ($i<int(@currPath))&&(!$found); $i++) {
                if ((int(@path)<=$i) || ($currPath[$i] ne $path[$i])) {
                    $currIdx = $i;
                    $found = 1;
                }
            }

            if ($found) {
                for($i=(int(@currPath)-1); $i>=$currIdx; $i--) {
                    $kml .= "      </Folder>\n";
                    pop @currPath;
                }
            }

            $currIdx = int(@currPath);

            for($i=$currIdx; $i<int(@path); $i++) {
                my $s = "";
                $s .= "      <Folder>\n";
                $s .= "        <name>" . $path[$i] . "</name>\n";
                if ($i == int(@path)-1) {
                    $s .= kmlLookAt($sdRegion);
                }
                $s .= "        <visibility>1</visibility>\n";

                $kml .= $s;

                push @currPath, $path[$i];
            }

            my $s = "";

            if (int(@belist)) {

            my $imageDir = "${kmlTmp}/${sd}/BE";
            if (!(-e "${imageDir}")) {
                mkpath("${imageDir}", 0, 0755);
                if (!(-e "${imageDir}")) {
                    die "ERROR: Unable to create directory: ${imageDir} : $!\n";
                }
            } else {
                die "ERROR: directory: ${imageDir} already exists : $!\n";
            }

            $s .= "      <Folder>\n";
            $s .= "        <name>Bare Earth</name>\n";
            $s .= kmlLookAt($beRegion);
            $s .= "        <visibility>1</visibility>\n";

if ($inclKMLBoundingBoxes) {
            $s .= "      <Folder>\n";
            $s .= "        <name>Rect Bounding Box</name>\n";
            $s .= "        <visibility>1</visibility>\n";
            my $beIdx;
            for($beIdx=0; $beIdx<int(@belist); $beIdx++) {
                my $minLonStr = sprintf("%.10f", $beData->{"region_${beIdx}"}->{minLon});
                my $maxLonStr = sprintf("%.10f", $beData->{"region_${beIdx}"}->{maxLon});
                my $minLatStr = sprintf("%.10f", $beData->{"region_${beIdx}"}->{minLat});
                my $maxLatStr = sprintf("%.10f", $beData->{"region_${beIdx}"}->{maxLat});

                $s .= "        <Placemark>\n";
                $s .= "            <name>$belist[$beIdx]</name>\n";
                $s .= "            <visibility>1</visibility>\n";
                $s .= "            <styleUrl>#transBluePoly</styleUrl>\n";
                $s .= "            <Polygon>\n";
                $s .= "                <altitudeMode>clampToGround</altitudeMode>\n";
                $s .= "                <outerBoundaryIs>\n";
                $s .= "                    <LinearRing>\n";
                $s .= "                        <coordinates>\n";
                $s .= "                            $maxLonStr,$maxLatStr\n";
                $s .= "                            $maxLonStr,$minLatStr\n";
                $s .= "                            $minLonStr,$minLatStr\n";
                $s .= "                            $minLonStr,$maxLatStr\n";
                $s .= "                        </coordinates>\n";
                $s .= "                    </LinearRing>\n";
                $s .= "                </outerBoundaryIs>\n";
                $s .= "            </Polygon>\n";
                $s .= "        </Placemark>\n";
            }
            $s .= "      </Folder>\n";
}

if ($inclKMLBoundingBoxes) {
            $s .= "      <Folder>\n";
            $s .= "        <name>Data Coverage</name>\n";
            $s .= "        <visibility>1</visibility>\n";
}
            for($beIdx=0; $beIdx<int(@belist); $beIdx++) {
                my $minLonStr = sprintf("%.10f", $beData->{"region_${beIdx}"}->{minLon});
                my $maxLonStr = sprintf("%.10f", $beData->{"region_${beIdx}"}->{maxLon});
                my $minLatStr = sprintf("%.10f", $beData->{"region_${beIdx}"}->{minLat});
                my $maxLatStr = sprintf("%.10f", $beData->{"region_${beIdx}"}->{maxLat});

                my $imageBEFile = $belist[$beIdx];
                $imageBEFile =~ s/\.tif$/.png/;

                my $cmd = "cp ${beDir}/${imageBEFile} $imageDir";
                print LOGFILE execute_command($cmd, $interactive);

                $s .= "        <GroundOverlay>\n";
                $s .= "            <name>$belist[$beIdx]</name>\n";

                $s .= kmlLookAt($beData->{"region_${beIdx}"});

                $s .= "            <visibility>1</visibility>\n";
                $s .= "            <color>80ffffff</color>\n";
                $s .= "            <Icon>\n";
                $s .= "                <href>${sd}/BE/${imageBEFile}</href>\n";
                $s .= "            </Icon>\n";
                $s .= "            <LatLonBox>\n";
                $s .= "                <north>${maxLatStr}</north>\n";
                $s .= "                <south>${minLatStr}</south>\n";
                $s .= "                <east>${minLonStr}</east>\n";
                $s .= "                <west>${maxLonStr}</west>\n";
                $s .= "            </LatLonBox>\n";
                $s .= "        </GroundOverlay>\n";
            }
if ($inclKMLBoundingBoxes) {
            $s .= "      </Folder>\n";
}

            $s .= "      </Folder>\n";
            }

            if (int(@bldglist)) {

                my $imageDir = "${kmlTmp}/${sd}/BLD3";
                if (!(-e "${imageDir}")) {
                    mkpath("${imageDir}", 0, 0755);
                    if (!(-e "${imageDir}")) {
                        die "ERROR: Unable to create directory: ${imageDir} : $!\n";
                    }
                } else {
                    die "ERROR: directory: ${imageDir} already exists : $!\n";
                }

                $s .= "      <Folder>\n";
                $s .= "        <name>3D Buildings</name>\n";
                $s .= kmlLookAt($bldgRegion);
                $s .= "        <visibility>1</visibility>\n";

if ($inclKMLBoundingBoxes) {
                $s .= "      <Folder>\n";
                $s .= "        <name>Rect Bounding Box</name>\n";
                $s .= "        <visibility>1</visibility>\n";
                my $bldgIdx;
                for($bldgIdx=0; $bldgIdx<int(@bldglist); $bldgIdx++) {
                    my $minLonStr = sprintf("%.10f", $bldgData->{"region_${bldgIdx}"}->{minLon});
                    my $maxLonStr = sprintf("%.10f", $bldgData->{"region_${bldgIdx}"}->{maxLon});
                    my $minLatStr = sprintf("%.10f", $bldgData->{"region_${bldgIdx}"}->{minLat});
                    my $maxLatStr = sprintf("%.10f", $bldgData->{"region_${bldgIdx}"}->{maxLat});

                    $s .= "        <Placemark>\n";
                    $s .= "            <name>$bldglist[$bldgIdx]</name>\n";
                    $s .= "            <visibility>1</visibility>\n";
                    $s .= "            <styleUrl>#transBluePoly</styleUrl>\n";
                    $s .= "            <Polygon>\n";
                    $s .= "                <altitudeMode>clampToGround</altitudeMode>\n";
                    $s .= "                <outerBoundaryIs>\n";
                    $s .= "                    <LinearRing>\n";
                    $s .= "                        <coordinates>\n";
                    $s .= "                            $maxLonStr,$maxLatStr\n";
                    $s .= "                            $maxLonStr,$minLatStr\n";
                    $s .= "                            $minLonStr,$minLatStr\n";
                    $s .= "                            $minLonStr,$maxLatStr\n";
                    $s .= "                        </coordinates>\n";
                    $s .= "                    </LinearRing>\n";
                    $s .= "                </outerBoundaryIs>\n";
                    $s .= "            </Polygon>\n";
                    $s .= "        </Placemark>\n";
                }
                $s .= "      </Folder>\n";
}

if ($inclKMLBoundingBoxes) {
                $s .= "      <Folder>\n";
                $s .= "        <name>Data Coverage</name>\n";
                $s .= "        <visibility>1</visibility>\n";
}
                for($bldgIdx=0; $bldgIdx<int(@bldglist); $bldgIdx++) {
                    my $minLonStr = sprintf("%.10f", $bldgData->{"region_${bldgIdx}"}->{minLon});
                    my $maxLonStr = sprintf("%.10f", $bldgData->{"region_${bldgIdx}"}->{maxLon});
                    my $minLatStr = sprintf("%.10f", $bldgData->{"region_${bldgIdx}"}->{minLat});
                    my $maxLatStr = sprintf("%.10f", $bldgData->{"region_${bldgIdx}"}->{maxLat});

                    my $imageBldgFile = $bldglist[$bldgIdx];
                    $imageBldgFile =~ s/\.shp$/.png/;

                    my $cmd = "cp ${bldgDir}/${imageBldgFile} $imageDir";
                    print LOGFILE execute_command($cmd, $interactive);

                    $s .= "        <GroundOverlay>\n";
                    $s .= "            <name>$bldglist[$bldgIdx]</name>\n";

                    $s .= kmlLookAt($bldgData->{"region_${bldgIdx}"});

                    $s .= "            <visibility>1</visibility>\n";
                    $s .= "            <color>80ffffff</color>\n";
                    $s .= "            <Icon>\n";
                    $s .= "                <href>${sd}/BLD3/${imageBldgFile}</href>\n";
                    $s .= "            </Icon>\n";
                    $s .= "            <LatLonBox>\n";
                    $s .= "                <north>${maxLatStr}</north>\n";
                    $s .= "                <south>${minLatStr}</south>\n";
                    $s .= "                <east>${minLonStr}</east>\n";
                    $s .= "                <west>${maxLonStr}</west>\n";
                    $s .= "            </LatLonBox>\n";
                    $s .= "        </GroundOverlay>\n";
                }
if ($inclKMLBoundingBoxes) {
                $s .= "      </Folder>\n";
}

                $s .= "      </Folder>\n";
            }

            if (int(@bldg2list)) {

                my $imageDir = "${kmlTmp}/${sd}/BLD2";
                if (!(-e "${imageDir}")) {
                    mkpath("${imageDir}", 0, 0755);
                    if (!(-e "${imageDir}")) {
                        die "ERROR: Unable to create directory: ${imageDir} : $!\n";
                    }
                } else {
                    die "ERROR: directory: ${imageDir} already exists : $!\n";
                }

                $s .= "      <Folder>\n";
                $s .= "        <name>2D Buildings</name>\n";
                $s .= kmlLookAt($bldg2Region);
                $s .= "        <visibility>1</visibility>\n";
if ($inclKMLBoundingBoxes) {
                $s .= "      <Folder>\n";
                $s .= "        <name>Rect Bounding Box</name>\n";
                $s .= "        <visibility>1</visibility>\n";
                my $bldg2Idx;
                for($bldg2Idx=0; $bldg2Idx<int(@bldg2list); $bldg2Idx++) {
                    my $minLonStr = sprintf("%.10f", $bldg2Data->{"region_${bldg2Idx}"}->{minLon});
                    my $maxLonStr = sprintf("%.10f", $bldg2Data->{"region_${bldg2Idx}"}->{maxLon});
                    my $minLatStr = sprintf("%.10f", $bldg2Data->{"region_${bldg2Idx}"}->{minLat});
                    my $maxLatStr = sprintf("%.10f", $bldg2Data->{"region_${bldg2Idx}"}->{maxLat});

                    $s .= "        <Placemark>\n";
                    $s .= "            <name>$bldg2list[$bldg2Idx]</name>\n";
                    $s .= "            <visibility>1</visibility>\n";
                    $s .= "            <styleUrl>#transBluePoly</styleUrl>\n";
                    $s .= "            <Polygon>\n";
                    $s .= "                <altitudeMode>clampToGround</altitudeMode>\n";
                    $s .= "                <outerBoundaryIs>\n";
                    $s .= "                    <LinearRing>\n";
                    $s .= "                        <coordinates>\n";
                    $s .= "                            $maxLonStr,$maxLatStr\n";
                    $s .= "                            $maxLonStr,$minLatStr\n";
                    $s .= "                            $minLonStr,$minLatStr\n";
                    $s .= "                            $minLonStr,$maxLatStr\n";
                    $s .= "                        </coordinates>\n";
                    $s .= "                    </LinearRing>\n";
                    $s .= "                </outerBoundaryIs>\n";
                    $s .= "            </Polygon>\n";
                    $s .= "        </Placemark>\n";
                }
                $s .= "      </Folder>\n";
}

if ($inclKMLBoundingBoxes) {
                $s .= "      <Folder>\n";
                $s .= "        <name>Data Coverage</name>\n";
                $s .= "        <visibility>1</visibility>\n";
}
                for($bldg2Idx=0; $bldg2Idx<int(@bldg2list); $bldg2Idx++) {
                    my $minLonStr = sprintf("%.10f", $bldg2Data->{"region_${bldg2Idx}"}->{minLon});
                    my $maxLonStr = sprintf("%.10f", $bldg2Data->{"region_${bldg2Idx}"}->{maxLon});
                    my $minLatStr = sprintf("%.10f", $bldg2Data->{"region_${bldg2Idx}"}->{minLat});
                    my $maxLatStr = sprintf("%.10f", $bldg2Data->{"region_${bldg2Idx}"}->{maxLat});

                    my $imageBldgFile = $bldg2list[$bldg2Idx];
                    $imageBldgFile =~ s/\.shp$/.png/;

                    my $cmd = "cp ${bldg2Dir}/${imageBldgFile} $imageDir";
                    print LOGFILE execute_command($cmd, $interactive);

                    $s .= "        <GroundOverlay>\n";
                    $s .= "            <name>$bldg2list[$bldg2Idx]</name>\n";

                    $s .= kmlLookAt($bldg2Data->{"region_${bldg2Idx}"});

                    $s .= "            <visibility>1</visibility>\n";
                    $s .= "            <color>80ffffff</color>\n";
                    $s .= "            <Icon>\n";
                    $s .= "                <href>${sd}/BLD2/${imageBldgFile}</href>\n";
                    $s .= "            </Icon>\n";
                    $s .= "            <LatLonBox>\n";
                    $s .= "                <north>${maxLatStr}</north>\n";
                    $s .= "                <south>${minLatStr}</south>\n";
                    $s .= "                <east>${minLonStr}</east>\n";
                    $s .= "                <west>${maxLonStr}</west>\n";
                    $s .= "            </LatLonBox>\n";
                    $s .= "        </GroundOverlay>\n";
                }
if ($inclKMLBoundingBoxes) {
                $s .= "      </Folder>\n";
}

                $s .= "      </Folder>\n";
            }

            $kml .= $s;
        }

        if ($processLidarFlag) {
        #########################################################################
        # Match single BE files and BLDG files by looking at how coverage areas overlap
        #########################################################################
        my @beIdxList = (0..int(@belist)-1);
        my @bldgIdxList = (0..int(@bldglist)+int(@bldg2list)-1);
        my @matchList = ();
        my $i = 0;
        my $k = 0;
        my $bldgIdx;
        my $bldg2Idx;

        while($i < int(@beIdxList)) {
            $beIdx = $beIdxList[$i];
            my $foundMatch = 0;
            for($k=0; $k<int(@bldgIdxList)&&(!$foundMatch); $k++) {
                $bldgIdx = $bldgIdxList[$k];
                if ($bldgIdx < int(@bldglist)) {
                    my $mstr = "";
                    $foundMatch = isMatch($beData->{"region_${beIdx}"}, $bldgData->{"region_${bldgIdx}"}, \$mstr);
                    print LOGFILE $mstr;
                } else {
                    my $mstr = "";
                    $bldg2Idx = $bldgIdx - int(@bldglist);
                    $foundMatch = isMatch($beData->{"region_${beIdx}"}, $bldg2Data->{"region_${bldg2Idx}"}, \$mstr);
                    print LOGFILE $mstr;
                }
                if ($foundMatch) {
                    push @matchList, "$beIdx:$bldgIdx";
                    splice @beIdxList, $i, 1;
                    splice @bldgIdxList, $k, 1;
                }
            }
            if (!$foundMatch) {
                $i++;
            }
        }

#        my $mIdx;
#        print LOGFILE "Matches Found: " . int(@matchList) . "\n";
#        for($mIdx=0; $mIdx<int(@matchList); $mIdx++) {
#            ($beIdx, $bldgIdx) = split(":", $matchList[$mIdx]);
#            if ($bldgIdx < int(@bldglist)) {
#                print LOGFILE "    ($mIdx) " . $belist[$beIdx] . " " . $bldglist[$bldgIdx] . "\n";
#            } else {
#                $bldg2Idx = $bldgIdx - int(@bldglist);
#                print LOGFILE "    ($mIdx) " . $belist[$beIdx] . " " . $bldg2list[$bldg2Idx] . "\n";
#            }
#        }
        #########################################################################

        #########################################################################
        # Match groups of BE files and BLDG files by looking at how coverage areas overlap
        #########################################################################
        {
            my $i = 0;
            while($i < @beIdxList) {
                my @matchBE = ($i);
                my @matchBLDG = ();
                my $nBE = 0;   # number of BE   that have been searched for overlapping BLDG
                my $nBLDG = 0; # number of BLDG that have been searched for overlapping BE
                while( ($nBE < @matchBE) || ($nBLDG < @matchBLDG) ) {
                    if ($nBE < @matchBE) {
                        my $i2 = $matchBE[$nBE];
                        $beIdx = $beIdxList[$i2];
                        for(my $k=0; $k<@bldgIdxList; $k++) {
                            my $contains = 0;
                            for(my $pk=0; ($pk<@matchBLDG)&&(!$contains); $pk++) {
                                if ($matchBLDG[$pk] == $k) { $contains = 1; }
                            }
                            if (!$contains) {
                                $bldgIdx = $bldgIdxList[$k];
                                my $overlap;
                                if ($bldgIdx < int(@bldglist)) {
                                    $overlap = regionOverlap($beData->{"region_${beIdx}"}, $bldgData->{"region_${bldgIdx}"});
                                } else {
                                    $bldg2Idx = $bldgIdx - int(@bldglist);
                                    $overlap = regionOverlap($beData->{"region_${beIdx}"}, $bldg2Data->{"region_${bldg2Idx}"});
                                }
                                if ($overlap) {
                                    push @matchBLDG, $k;
                                }
                            }
                        }
                        $nBE++;
                    }
                    if ($nBLDG < @matchBLDG) {
                        my $k2 = $matchBLDG[$nBLDG];
                        $bldgIdx = $bldgIdxList[$k2];
                        for(my $i2=0; $i2<@beIdxList; $i2++) {
                            my $contains = 0;
                            for(my $ppi=0; ($ppi<int(@matchBE))&&(!$contains); $ppi++) {
                                if ($matchBE[$ppi] == $i2) { $contains = 1; }
                            }
                            if (!$contains) {
                                $beIdx = $beIdxList[$i2];
                                my $overlap;
                                if ($bldgIdx < int(@bldglist)) {
                                    $overlap = regionOverlap($beData->{"region_${beIdx}"}, $bldgData->{"region_${bldgIdx}"});
                                } else {
                                    $bldg2Idx = $bldgIdx - int(@bldglist);
                                    $overlap = regionOverlap($beData->{"region_${beIdx}"}, $bldg2Data->{"region_${bldg2Idx}"});
                                }
                                if ($overlap) {
                                    push @matchBE, $i2;
                                }
                            }
                        }
                        $nBLDG++;
                    }
                }
                if ($nBLDG) {
                    my @matchBEIdxList   = map { $beIdxList[$_]   } @matchBE;
                    my @matchBLDGIdxList = map { $bldgIdxList[$_] } @matchBLDG;
                    my $matchBEStr = join(",", @matchBEIdxList);
                    my $matchBLDGStr = join(",", @matchBLDGIdxList);
                    my $matchStr = "${matchBEStr}:${matchBLDGStr}";
                    push @matchList, $matchStr;
                    my @sortMatchBE = sort { $a <=> $b } @matchBE;
                    for(my $i2=int(@sortMatchBE)-1; $i2>=0; $i2--) {
                        splice @beIdxList, $sortMatchBE[$i2], 1;
                    }
                    my @sortMatchBLDG = sort { $a <=> $b } @matchBLDG;
                    for(my $i2=int(@sortMatchBLDG)-1; $i2>=0; $i2--) {
                        splice @bldgIdxList, $sortMatchBLDG[$i2], 1;
                    }
                } else {
                    $i++;
                }
            }
        }
        #########################################################################

        print LOGFILE "\n";
                            
        #########################################################################
        # Print matched and unmatched files                                     #
        #########################################################################
        print LOGFILE "Subdirectory: ${sd}\n";
        my $mIdx;
        print LOGFILE "Matches Found: " . int(@matchList) . "\n";
        for($mIdx=0; $mIdx<int(@matchList); $mIdx++) {
            my $beStr;
            my $bldgStr;
            ($beStr, $bldgStr) = split(":", $matchList[$mIdx]);
            my @matchBEIdxList = split(",", $beStr);
            my @matchBLDGIdxList = split(",", $bldgStr);

            print LOGFILE "    Match ${mIdx}:\n";
            print LOGFILE "        Number of Bare-Earth Files: " . int(@matchBEIdxList) . "\n";
            for(my $pIdx=0; $pIdx<@matchBEIdxList; $pIdx++) {
                $beIdx = $matchBEIdxList[$pIdx];
                print LOGFILE "            ($pIdx) " . $belist[$beIdx] . "\n";
            }
            print LOGFILE "        Number of Building Files: " . int(@matchBLDGIdxList) . "\n";
            for(my $pIdx=0; $pIdx<@matchBLDGIdxList; $pIdx++) {
                $bldgIdx = $matchBLDGIdxList[$pIdx];
                if ($bldgIdx < int(@bldglist)) {
                    print LOGFILE "            ($pIdx) " . $bldglist[$bldgIdx] . "\n";
                } else {
                    $bldg2Idx = $bldgIdx - int(@bldglist);
                    print LOGFILE "            ($pIdx) " . $bldg2list[$bldg2Idx] . "\n";
                }
            }
        }
        print LOGFILE "\n";

        print LOGFILE "Unmatched BE files: " . int(@beIdxList) . "\n";
        for($i=0; $i < int(@beIdxList); $i++) {
            $beIdx = $beIdxList[$i];
            my $discFile = "${sd}/BE/" . $belist[$beIdx];
            print LOGFILE "    ($i) $discFile\n";
            $discard .= "$discFile : unmatched\n";
        }
        print LOGFILE "\n";
        print LOGFILE "Unmatched BLDG files: " . int(@bldgIdxList) . "\n";
        for($k=0; $k < int(@bldgIdxList); $k++) {
            $bldgIdx = $bldgIdxList[$k];
            if ($bldgIdx < int(@bldglist)) {
                my $discFile = "${sd}/VEC/BLD3/" . $bldglist[$bldgIdx];
                print LOGFILE "    ($k) $discFile\n";
                $discard .= "$discFile : unmatched\n";
            } else {
                $bldg2Idx = $bldgIdx - int(@bldglist);
                my $discFile = "${sd}/VEC/BLD2/" . $bldg2list[$bldg2Idx];
                print LOGFILE "    ($k) $discFile\n";
                $discard .= "$discFile : unmatched\n";
            }
        }
        print LOGFILE "\n";
        #########################################################################

        #########################################################################
        # Process matched files
        #########################################################################
        for($mIdx=0; $mIdx<int(@matchList); $mIdx++) {
            my $beStr;
            my $bldgStr;
            ($beStr, $bldgStr) = split(":", $matchList[$mIdx]);
            my @matchBEIdxList = split(",", $beStr);
            my @matchBLDGIdxList = split(",", $bldgStr);

            my $cityNameLC = lc $cityName;
            my @delList = ();

            $cmd = "gdalbuildvrt ${tmpDir}/${cityNameLC}_bare_earth_${mIdx}.vrt";
            for($i=0; $i < int(@matchBEIdxList); $i++) {
                $beIdx = $matchBEIdxList[$i];
                $cmd .= " " . $beDir . "/" . $belist[$beIdx];
            }
            $cmd .= " -srcnodata $nodataVal";
            print LOGFILE execute_command($cmd, $interactive);
            push @delList, "${tmpDir}/${cityNameLC}_bare_earth_${mIdx}.vrt";

            $cmd = "gdalwarp -t_srs '+proj=longlat +datum=WGS84' -tr $lonlatRes $lonlatRes -co \"TILED=YES\" "
                 . "${tmpDir}/${cityNameLC}_bare_earth_${mIdx}.vrt ${tmpDir}/${cityNameLC}_bare_earth_${mIdx}.tif";
            print LOGFILE execute_command($cmd, $interactive);
            push @delList, "${tmpDir}/${cityNameLC}_bare_earth_${mIdx}.tif";

            my $beInfo = `gdalinfo -norat -nomd -noct ${tmpDir}/${cityNameLC}_bare_earth_${mIdx}.tif`;

            my $llLon;
            my $llLat;
            my $urLon;
            my $urLat;
            if ($beInfo =~ /.*Lower Left\s*\(\s*([\d.-]*)\s*,\s*([\d.-]*)\s*\).*\nUpper Right\s*\(\s*([\d.-]*)\s*,\s*([\d.-]*)\s*\)/m) {
                $llLon = $1;
                $llLat = $2;
                $urLon = $3;
                $urLat = $4;
            } else {
                die "ERROR: Unable to extract coverage area from converted bare earth file: ${tmpDir}/${cityNameLC}_bare_earth_${mIdx}.tif : $!\n";
            }

            my $xsize;
            my $ysize;
            if ($beInfo =~ /\n\s*Size is\s+(\d+),\s*(\d+)\s*\s/m) {
                $xsize = $1;
                $ysize = $2;
            } else {
                die "ERROR: Unable to extract xsize and ysize from converted bare earth file: ${tmpDir}/${cityNameLC}_bare_earth_${mIdx}.tif : $!\n";
            }

            my $fileList = "";
            for($k=0; $k < int(@matchBLDGIdxList); $k++) {
                $bldgIdx = $matchBLDGIdxList[$k];
                if ($bldgIdx < int(@bldglist)) {
                    # my $aName = $bldgData->{"region_${bldgIdx}"}->{aName};
                    my $aName = $heightFieldName;
                    if ($aName eq $heightFieldName) {
                        $fileList .= " " . $bldgDir . "/" . $bldglist[$bldgIdx];
                    } else {
                        my $fileA = $bldgDir . "/" . $bldglist[$bldgIdx];
                        my $fileB = $tmpDir . "/r_" . $bldglist[$bldgIdx];
                        my $layerName = substr $bldglist[$bldgIdx], 0, -4;

                        $cmd = "ogr2ogr $fileB $fileA -sql \"SELECT $aName A $heightFieldName from \\\"$layerName\\\"\"";
                        print LOGFILE execute_command($cmd, $interactive);
                        push @delList, $fileB;

                        $fileList .= " " . $fileB;
                    }
                } else {
                    $bldg2Idx = $bldgIdx - int(@bldglist);
                    # my $aName = $bldg2Data->{"region_${bldg2Idx}"}->{aName};
                    my $aName = $heightFieldName;
                    if ($aName eq $heightFieldName) {
                        $fileList .= " " . $bldg2Dir . "/" . $bldg2list[$bldg2Idx];
                    } else {
                        my $fileA = $bldg2Dir . "/" . $bldg2list[$bldg2Idx];
                        my $fileB = $tmpDir . "/r_" . $bldg2list[$bldg2Idx];
                        my $layerName = substr $bldg2list[$bldg2Idx], 0, -4;

                        $cmd = "ogr2ogr $fileB $fileA -sql \"SELECT $aName AS $heightFieldName from \\\"$layerName\\\"\"";
                        print LOGFILE execute_command($cmd, $interactive);
                        push @delList, $fileB;

                        $fileList .= " " . $fileB;
                    }
                }
            }

            $cmd = "ogrmerge.py -single -o ${tmpDir}/${cityNameLC}_3d_buildings_${mIdx}.vrt $fileList";
            print LOGFILE execute_command($cmd, $interactive);
            push @delList, "${tmpDir}/${cityNameLC}_3d_buildings_${mIdx}.vrt";

            $cmd = "ogr2ogr -f SQLite -t_srs 'WGS84' ${tmpDir}/${cityNameLC}_3d_buildings_${mIdx}.sqlite3 ${tmpDir}/${cityNameLC}_3d_buildings_${mIdx}.vrt -dsco SPATIALITE=YES";
            print LOGFILE execute_command($cmd, $interactive);
            push @delList, "${tmpDir}/${cityNameLC}_3d_buildings_${mIdx}.sqlite3";

            $cmd = "gdal_rasterize -sql \"SELECT * FROM merged ORDER BY $heightFieldName \" "
                 . "-a $heightFieldName "
                 . "-of GTiff "
                 . "-co \"TILED=YES\" "
                 . "-ot Float32 "
                 . "-a_nodata $nodataVal "
                 . "-tr $lonlatRes $lonlatRes "
                 . "-a_srs WGS84 "
                 . "-te $llLon $llLat $urLon $urLat "
                 . "${tmpDir}/${cityNameLC}_3d_buildings_${mIdx}.sqlite3 "
                 . "${tmpDir}/${cityNameLC}_3d_buildings_${mIdx}.tif";
            print LOGFILE execute_command($cmd, $interactive);
            push @delList, "${tmpDir}/${cityNameLC}_3d_buildings_${mIdx}.tif";

            $cmd = "gdalbuildvrt -separate ${tmpDir}/${cityNameLC}_${mIdx}.vrt ${tmpDir}/${cityNameLC}_bare_earth_${mIdx}.tif ${tmpDir}/${cityNameLC}_3d_buildings_${mIdx}.tif";
            print LOGFILE execute_command($cmd, $interactive);
            push @delList, "${tmpDir}/${cityNameLC}_${mIdx}.vrt";

            if (!(-e "${destDir}/${sd}")) {
                mkpath("${destDir}/${sd}", 0, 0755);
                if (!(-e "$destDir/${sd}")) {
                    die "ERROR: Unable to create directory: ${destDir}/${sd} : $!\n";
                }
            }

            my $nx = (($xsize+$gpMaxPixel-1) - (($xsize+$gpMaxPixel-1)%$gpMaxPixel))/$gpMaxPixel;
            my $ny = (($ysize+$gpMaxPixel-1) - (($ysize+$gpMaxPixel-1)%$gpMaxPixel))/$gpMaxPixel;
            my $tileIdx = 0;
            my $ix;
            my $iy;
            my $oy = 0;
            for($iy=0; $iy<$ny; $iy++) {
                my $dy = (($ysize + $iy) - (($ysize + $iy) % $ny))/$ny;
                my $ox = 0;
                for($ix=0; $ix<$nx; $ix++) {
                    $tileIdx++;
                    my $tstr = sprintf("%.2d", $tileIdx);

                    my $dx = (($xsize + $ix) - (($xsize + $ix) % $nx))/$nx;

                    $cmd = "gdal_translate -srcwin $ox $oy $dx $dy -co \"TILED=YES\" ${tmpDir}/${cityNameLC}_${mIdx}.vrt ${destDir}/${sd}/${cityNameLC}_${mIdx}_${tstr}.tif";
                    print LOGFILE execute_command($cmd, $interactive);

                    $ox += $dx;

                    my $tifInfo = `gdalinfo -norat -nomd -noct ${destDir}/${sd}/${cityNameLC}_${mIdx}_${tstr}.tif`;

                    my $llLon;
                    my $llLat;
                    my $urLon;
                    my $urLat;
                    if ($tifInfo =~ /.*Lower Left\s*\(\s*([\d.-]*)\s*,\s*([\d.-]*)\s*\).*\nUpper Right\s*\(\s*([\d.-]*)\s*,\s*([\d.-]*)\s*\)/m) {
                        $llLon = $1;
                        $llLat = $2;
                        $urLon = $3;
                        $urLat = $4;
                    } else {
                        die "ERROR: Unable to extract coverage area from tif file: ${destDir}/${sd}/${cityNameLC}_${mIdx}_${tstr}.tif : $!\n";
                    }

                    print INFOFILE "${relSD}${cityNameLC}_${mIdx}_${tstr}.tif,$llLon,$urLon,$llLat,$urLat\n";
                }
                $oy += $dy;
            }

            if ($cleantmp) {
                my $delFile;
                foreach $delFile (@delList) {
                    unlink("$delFile") or die "ERROR: Unable to delete file \"$delFile\" : $!\n";
                }
            }
        }
        #########################################################################
        }
    }

    if ($cleantmp) {
        my $fixedTmpDir = "${tmpDir}/fixed/${cityName}";
        if (-e $fixedTmpDir) {
            print LOGFILE "Removing Directory \"$fixedTmpDir\" ... \n";
            rmtree(${fixedTmpDir});
        }
    }

    if ($srcKMLFlag) {
        my $i;
        for($i=(int(@currPath)-1); $i>=0; $i--) {
            if ($i == 0) {
                $kml .= kmlLookAt($cityRegion);
            }
            $kml .= "      </Folder>\n";
            pop @currPath;
        }
    }

    if ($processLidarFlag) {
        close INFOFILE;
    }

    my $t1 = time;
    print "Completed: $cityName, ELAPSED TIME = " . ($t1-$t0) . " sec\n";

    print LOGFILE "Done Pocessing City: $cityName, ELAPSED TIME = " . ($t1-$t0) . " sec\n";
    close LOGFILE;

    $kmlhash{$cityStr} = $kml;
    $discardhash{$cityStr} = $discard;

    my $log;
    open(INFILE, "$logFile")    || die "Can't open file $logFile  : $!\n";
    binmode INFILE;

    my ($savedreadstate) = $/;
    undef $/;
    $log=<INFILE>;
    $/ = $savedreadstate;
    close INFILE;

    $loghash{$cityStr} = $log;

    return;
}
#################################################################################

#################################################################################
# Routine fixRaster
#################################################################################
sub fixRaster
{
    my $inFile = $_[0];
    my $outFile = $_[1];
    my $imageFile = $_[2];
    my $tmpImageFile = $_[3];
    my $p = $_[4];
    my $cityName = $_[5];

    my $templFile = "/tmp/templ_proc_gdal_${cityName}_$$.txt";

    my $s = "";
    my $paramIdx = 0;

    $s .= "PARAM_${paramIdx}: FUNCTION \"fixRaster\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: SRC_FILE_RASTER \"$inFile\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: OUTPUT_FILE \"$outFile\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: NODATA_VAL $nodataVal\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: CLAMP_MIN $p->{clampMin}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: CLAMP_MAX $p->{clampMax}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: MIN_MAG $p->{minMag}\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: IMAGE_FILE \"$imageFile\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: TMP_IMAGE_FILE \"$tmpImageFile\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: IMAGE_LON_LAT_RES $imageLonLatRes\n";
    $paramIdx++;

    my $numParam = $paramIdx;

    open(FILE, ">$templFile") || die "Can't open file $templFile : $!\n";
    print FILE "# proc_gdal TEMPLATE FILE\n";
    print FILE "FORMAT: 1_0\n";
    print FILE "\n";
    print FILE "NUM_PARAM: ${numParam}\n";
    print FILE "\n";
    print FILE "$s";
    close FILE;

    my $resp = `./proc_gdal -templ $templFile 2>&1`;

    return($resp);
}
#################################################################################

#################################################################################
# Routine fixVector
#################################################################################
sub fixVector
{
    my $inFile = $_[0];
    my $inHeightFieldName = $_[1];
    my $outFile = $_[2];
    my $imageFile = $_[3];
    my $tmpImageFile = $_[4];
    my $cityName = $_[5];

    my $templFile = "/tmp/templ_proc_gdal_${cityName}_$$.txt";

    my $s = "";
    my $paramIdx = 0;

    $s .= "PARAM_${paramIdx}: FUNCTION \"fixVector\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: SRC_FILE_VECTOR \"$inFile\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: SRC_HEIGHT_FIELD_NAME \"$inHeightFieldName\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: OUTPUT_FILE \"$outFile\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: OUTPUT_HEIGHT_FIELD_NAME \"$heightFieldName\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: IMAGE_FILE \"$imageFile\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: TMP_IMAGE_FILE \"$tmpImageFile\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: IMAGE_LON_LAT_RES $imageLonLatRes\n";
    $paramIdx++;

    my $numParam = $paramIdx;

    open(FILE, ">$templFile") || die "Can't open file $templFile : $!\n";
    print FILE "# proc_gdal TEMPLATE FILE\n";
    print FILE "FORMAT: 1_0\n";
    print FILE "\n";
    print FILE "NUM_PARAM: ${numParam}\n";
    print FILE "\n";
    print FILE "$s";
    close FILE;

    my $resp = `./proc_gdal -templ $templFile 2>&1`;

    return($resp);
}
#################################################################################

#################################################################################
# Routine vectorCvg
#################################################################################
sub vectorCvg
{
    my $inFile = $_[0];
    my $imageFile = $_[1];
    my $tmpImageFile = $_[2];

    my $templFile = "/tmp/templ_proc_gdal_$$.txt";

    my $s = "";
    my $paramIdx = 0;

    $s .= "PARAM_${paramIdx}: FUNCTION \"vectorCvg\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: SRC_FILE_VECTOR \"$inFile\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: IMAGE_FILE \"$imageFile\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: TMP_IMAGE_FILE \"$tmpImageFile\"\n";
    $paramIdx++;

    $s .= "PARAM_${paramIdx}: IMAGE_LON_LAT_RES $imageLonLatRes\n";
    $paramIdx++;

    my $numParam = $paramIdx;

    open(FILE, ">$templFile") || die "Can't open file $templFile : $!\n";
    print FILE "# proc_gdal TEMPLATE FILE\n";
    print FILE "FORMAT: 1_0\n";
    print FILE "\n";
    print FILE "NUM_PARAM: ${numParam}\n";
    print FILE "\n";
    print FILE "$s";
    close FILE;

    my $resp = `./proc_gdal -templ $templFile`;
    print $resp;

    return;
}
#################################################################################

#################################################################################
# Routine isMatch
#################################################################################
sub isMatch
{
    my $regionA = $_[0];
    my $regionB = $_[1];
    my $logref  = $_[2];
    my $match;

    if (($regionA->{maxLon} <= $regionB->{minLon}) || ($regionA->{minLon} >= $regionB->{maxLon})) {
        $match = 0;
    } elsif (($regionA->{maxLat} <= $regionB->{minLat}) || ($regionA->{minLat} >= $regionB->{maxLat})) {
        $match = 0;
    } else {
        my $overlapMinLon = List::Util::max($regionA->{minLon}, $regionB->{minLon});
        my $overlapMaxLon = List::Util::min($regionA->{maxLon}, $regionB->{maxLon});
        my $overlapMinLat = List::Util::max($regionA->{minLat}, $regionB->{minLat});
        my $overlapMaxLat = List::Util::min($regionA->{maxLat}, $regionB->{maxLat});
        my $areaA = ($regionA->{maxLon} - $regionA->{minLon})*($regionA->{maxLat} - $regionA->{minLat});
        my $areaB = ($regionB->{maxLon} - $regionB->{minLon})*($regionB->{maxLat} - $regionB->{minLat});
        my $areaOverlap = ($overlapMaxLon - $overlapMinLon)*($overlapMaxLat - $overlapMinLat);
        my $metric = $areaOverlap / ($areaA + $areaB - $areaOverlap);

        ${$logref} .= "METRIC = $metric\n";

        $match = ($metric >= 0.80 ? 1 : 0);
    }


    return $match;
}
#################################################################################

#################################################################################
# Routine regionOverlap
#################################################################################
sub regionOverlap
{
    my $regionA = $_[0];
    my $regionB = $_[1];
    my $overlap;

    if (($regionA->{maxLon} <= $regionB->{minLon}) || ($regionA->{minLon} >= $regionB->{maxLon})) {
        $overlap = 0;
    } elsif (($regionA->{maxLat} <= $regionB->{minLat}) || ($regionA->{minLat} >= $regionB->{maxLat})) {
        $overlap = 0;
    } else {
        $overlap = 1;
    }


    return $overlap;
}
#################################################################################

#################################################################################
# Routine regionUnion
#################################################################################
sub regionUnion
{
    my $regionA = $_[0];
    my $regionB = $_[1];
    my $unionRegion = {};

    if (exists($regionA->{minLon}) && exists($regionB->{minLon})) {
        $unionRegion->{minLon} = ($regionA->{minLon} < $regionB->{minLon} ? $regionA->{minLon} : $regionB->{minLon});
        $unionRegion->{maxLon} = ($regionA->{maxLon} > $regionB->{maxLon} ? $regionA->{maxLon} : $regionB->{maxLon});
        $unionRegion->{minLat} = ($regionA->{minLat} < $regionB->{minLat} ? $regionA->{minLat} : $regionB->{minLat});
        $unionRegion->{maxLat} = ($regionA->{maxLat} > $regionB->{maxLat} ? $regionA->{maxLat} : $regionB->{maxLat});
    } elsif (exists($regionB->{minLon})) {
        $unionRegion->{minLon} = $regionB->{minLon};
        $unionRegion->{maxLon} = $regionB->{maxLon};
        $unionRegion->{minLat} = $regionB->{minLat};
        $unionRegion->{maxLat} = $regionB->{maxLat};
    } elsif (exists($regionA->{minLon})) {
        $unionRegion->{minLon} = $regionA->{minLon};
        $unionRegion->{maxLon} = $regionA->{maxLon};
        $unionRegion->{minLat} = $regionA->{minLat};
        $unionRegion->{maxLat} = $regionA->{maxLat};
    }

    return $unionRegion;
}
#################################################################################


#################################################################################
# Routine kmlLookAt
#################################################################################
sub kmlLookAt
{
    my $region = $_[0];
    my $s = "";

    my $centerLon = ($region->{minLon} + $region->{maxLon})/2;
    my $centerLat = ($region->{minLat} + $region->{maxLat})/2;

    my $centerLonStr = sprintf("%.10f", $centerLon);
    my $centerLatStr = sprintf("%.10f", $centerLat);

    my $deltaLonRad = ($region->{maxLon}-$region->{minLon})*$pi/180.0;
    my $deltaLatRad = ($region->{maxLat}-$region->{minLat})*$pi/180.0;

    my $vDist = $deltaLatRad*$earthRadius;
    my $hDist = $deltaLonRad*$earthRadius*cos($centerLat*$pi/180.0);
    my $range = 2*($hDist > $vDist ? $hDist : $vDist);

    $s .= "                <LookAt>\n";
    $s .= "                    <longitude>${centerLonStr}</longitude>\n";
    $s .= "                    <latitude>${centerLatStr}</latitude>\n";
    $s .= "                    <altitude>0</altitude>\n";
    $s .= "                    <tilt>20.0</tilt>\n";
    $s .= "                    <range>$range</range>\n";
    $s .= "                    <altitudeMode>relativeToGround</altitudeMode>\n";
    $s .= "                </LookAt>\n";

    return $s;
}
#################################################################################

#################################################################################
# Routine extractLonLat
#################################################################################
sub extractLonLat
{
    my $str       = $_[0];
    my $lonVal;
    my $latVal;

    if ($str =~ /^\s*(\d+)d\s*(\d+)\'\s*([\d.]+)\"(E|W),\s*(\d+)d\s*(\d+)\'\s*([\d.]+)\"(N|S)$/) {
        $lonVal = ($1 + ($2 + $3/60)/60)*($4 eq "W" ? -1 : 1);
        $latVal = ($5 + ($6 + $7/60)/60)*($8 eq "S" ? -1 : 1);
    } else {
        die "ERROR: Unable to extract LON/LAT values from \"$str\" : $!\n";
    }

    return($lonVal, $latVal);
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
    my $retval = "";
    my $resp;

    my $exec_flag = 1;

    if ($interactive) {
        print "Are you sure you want to execute command: $cmd (y/n)? ";
        chop($resp = <STDIN>);
        if ($resp ne "y") {
            $exec_flag = 0;
        }
    }
    if ($exec_flag) {
        $retval .= "$cmd\n";
        $retval .= `$cmd 2>&1`;
        if ($? == -1) {
            die "failed to execute: $!\n";
        } elsif ($? & 127) {
            my $errmsg = sprintf("child died with signal %d, %s coredump\n", ($? & 127),  ($? & 128) ? 'with' : 'without');
            die "$errmsg : $!\n";
        }
    }

    return $retval;
}
#################################################################################

#################################################################################
# Routine genKML
#################################################################################
sub genKML
{
    my $codePart = $_[0];
    my $s = "";

    if ($codePart eq "HEAD") {
        $s .= "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        $s .= "<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n";
        $s .= "\n";
        $s .= "<Document>\n";
        $s .= "\n";
        $s .= "    <name>Source Lidar Data</name>\n";
        $s .= "    <open>1</open>\n";
        $s .= "    <description>Display Regions Covered by Source Lidar Data</description>\n";
        $s .= "\n";
        $s .= "    <Style id=\"transBluePoly\">\n";
        $s .= "        <LineStyle>\n";
        $s .= "            <width>1.5</width>\n";
        $s .= "        </LineStyle>\n";
        $s .= "        <PolyStyle>\n";
        $s .= "            <color>7dff0000</color>\n";
        $s .= "        </PolyStyle>\n";
        $s .= "    </Style>\n";
        $s .= "\n";
        $s .= "    <Style id=\"transBlackPoly\">\n";
        $s .= "        <LineStyle>\n";
        $s .= "            <width>1.5</width>\n";
        $s .= "        </LineStyle>\n";
        $s .= "        <PolyStyle>\n";
        $s .= "            <color>7d000000</color>\n";
        $s .= "        </PolyStyle>\n";
        $s .= "    </Style>\n";
    } elsif ($codePart eq "TAIL") {
        $s .= "</Document>\n";
        $s .= "</kml>\n";
    } else {
        die "ERROR: uncrecognized KML code part: \"$codePart\" : $!\n";
    }

    return $s;
}
#################################################################################

