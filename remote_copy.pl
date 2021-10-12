#!/usr/bin/perl -w
################################################################################
#### FILE: remote_copy.pl                                                   ####
#### Script to:                                                             ####
####     (1) Copy simulation results from AMZN servers to local computer.   ####
####     (2) Combine simulation results.                                    ####
####     (3) Process simulation results.                                    ####
#### Michael Mandell - 2018.11.07                                           ####
################################################################################

use File::Path;
use POSIX;
use strict;

$| = 1;

my $simCase = "UK_531_533";

my $copyFlag = 1;
my $combineFlag = 1;
my $processFlag = 1;

my $combineDir = "combined";
my $numFS;
my $numCombinedIterations = 250000;
my @serverList;
my $name;

if ($simCase eq "NL") {
    @serverList = ( "erik-kelvin.rkf-engineering.com:/home/mmandell/trunk",
                    "james-matlab.rkf-engineering.com:/home/mmandell/trunk"
                  );
    $name = "NL_250Kx_W2";
    $numFS = 26;
} elsif ($simCase eq "UK") {

    @serverList = ( "34.232.251.248:/home/mmandell/coalition_trunk",    # 0
                    "54.89.233.164:/home/mmandell/coalition_trunk",     # 1
                    "52.55.198.245:/home/mmandell/coalition_trunk",     # 2
                    "35.175.209.202:/home/mmandell/coalition_trunk",    # 3
                    "52.205.212.46:/home/mmandell/coalition_trunk",     # 4
                    "18.208.175.165:/home/mmandell/coalition_trunk",    # 5
                    "35.168.12.78:/home/mmandell/coalition_trunk",      # 6
                    "18.208.185.135:/home/mmandell/coalition_trunk",    # 7
                    "34.204.75.110:/home/mmandell/coalition_trunk",     # 8
                    "54.89.96.192:/home/mmandell/coalition_trunk",      # 9

                    "34.232.251.248:/home/mmandell/coalition_trunk/A",  # 0
                    "18.208.185.135:/home/mmandell/coalition_trunk/B",  # 7
                    "34.204.75.110:/home/mmandell/coalition_trunk/C",   # 8
                    "54.89.96.192:/home/mmandell/coalition_trunk/D"     # 9
                  );
    $name = "UK_250Kx_mid_W2";
    $numFS = 505;
} elsif ($simCase eq "UK_531_533") {

    @serverList = ( "34.232.251.248:/home/mmandell/coalition_trunk",    # 0
                    "54.157.47.141:/home/mmandell/coalition_trunk",     # 1
                    "54.198.8.25:/home/mmandell/coalition_trunk",       # 2
                    "54.81.94.139:/home/mmandell/coalition_trunk",      # 3
                    "54.242.109.239:/home/mmandell/coalition_trunk",    # 4
                    "54.92.201.27:/home/mmandell/coalition_trunk",      # 5
                    "34.229.118.126:/home/mmandell/coalition_trunk",    # 6
                    "35.175.192.246:/home/mmandell/coalition_trunk",    # 7
                    "52.90.106.186:/home/mmandell/coalition_trunk",     # 8
                    "34.207.100.149:/home/mmandell/coalition_trunk"     # 9
                  );
    $name = "UK_250Kx_W2_531_533";
    $numFS = 2;
} else {
    die "simCase = $simCase set to invalid value: $!\n";
}

my @sizeList = ("S", "M", "L");

my @mtgnList = ("", "_mtgn");

my $serverIdx;
my $sizeIdx;
my $mtgnIdx;
my @serverNumIterList;

################################################################################
# Copy Files                                                                   #
################################################################################
if ($copyFlag) {
    my $t1 = time;
    for($serverIdx=0; $serverIdx<@serverList; $serverIdx++) {
        my $server = $serverList[$serverIdx];
        for($sizeIdx=0; $sizeIdx<@sizeList; $sizeIdx++) {
            my $size = $sizeList[$sizeIdx];
            for($mtgnIdx=0; $mtgnIdx<@mtgnList; $mtgnIdx++) {
                my $mtgn = $mtgnList[$mtgnIdx];
                my $cmd = "scp -i /data/users/mmandell/id_rsa mmandell\@${server}/${name}_heatmap${mtgn}_${size}.csv ${name}_heatmap${mtgn}_${size}_${serverIdx}.csv";
                # print "$cmd\n";
                execute_command($cmd, 0);
            }
            my $cmd = "scp -i /data/users/mmandell/id_rsa mmandell\@${server}/${name}_exc_thr_wifi_${size}.csv ${name}_exc_thr_wifi_${size}_${serverIdx}.csv";
            execute_command($cmd, 0);
        }
    }
    my $t2 = time;
    print "COPY FILES COMPLETE, ELAPSED TIME = " . ($t2-$t1) . " sec\n";
}
################################################################################

################################################################################
# Determine number of iterations for each server                               #
################################################################################
my $totalNumIter;
if ($combineFlag) {
    my $t1 = time;
    $totalNumIter = 0;
    for($serverIdx=0; $serverIdx<@serverList; $serverIdx++) {
        my $server = $serverList[$serverIdx];
        my $initFlag = 1;
        my $numIter;
        for($sizeIdx=0; $sizeIdx<@sizeList; $sizeIdx++) {
            my $size = $sizeList[$sizeIdx];
            for($mtgnIdx=0; $mtgnIdx<@mtgnList; $mtgnIdx++) {
                my $mtgn = $mtgnList[$mtgnIdx];
                my $file = "${name}_heatmap${mtgn}_${size}_${serverIdx}.csv";
                my $numLines = (split(" ", `wc -l $file`))[0];
                my $ni = ( ($numLines - 1) - (($numLines - 1) % $numFS) ) / $numFS;
                if ( ($initFlag) || ($ni < $numIter) ) {
                    $numIter = $ni;
                    $initFlag = 0;
                }
            }
        }
        $serverNumIterList[$serverIdx] = $numIter;
        print "SERVER: $serverIdx, NUM_ITER: $serverNumIterList[$serverIdx]\n";

        $totalNumIter += $serverNumIterList[$serverIdx];
    }
    print "TOTAL NUM ITER: $totalNumIter\n";
    my $numExcessIter = $totalNumIter - $numCombinedIterations;

    if ($numExcessIter < 0) {
        print "Insufficient iterations, need " . (-$numExcessIter) . " additional iterations\n";
        exit;
    } else {
        print "NUM EXCESS ITER: $numExcessIter\n";
    }
    $serverIdx = int(@serverList)-1;
    while($serverNumIterList[$serverIdx] < $numExcessIter) {
        $numExcessIter -= $serverNumIterList[$serverIdx];
        $serverNumIterList[$serverIdx] = 0;
        $serverIdx--;
    }
    $serverNumIterList[$serverIdx] -= $numExcessIter;

    for($serverIdx=0; $serverIdx<@serverList; $serverIdx++) {
        print "SERVER: $serverIdx, NUM_ITER: $serverNumIterList[$serverIdx]\n";
    }
    my $t2 = time;
    print "COMBINE STEP 1, DETERMINE NUM ITER FOR EACH SERVER, COMPLETE, ELAPSED TIME = " . ($t2-$t1) . " sec\n";
}
################################################################################

################################################################################
# Combine Files                                                                #
################################################################################
if ($combineFlag) {
    my $t1 = time;
    if (!(-e $combineDir)) {
        my $cmd = "mkdir $combineDir";
        execute_command($cmd, 0);
    }

    for($sizeIdx=0; $sizeIdx<@sizeList; $sizeIdx++) {
        my $size = $sizeList[$sizeIdx];
        for($mtgnIdx=0; $mtgnIdx<@mtgnList; $mtgnIdx++) {
            my $mtgn = $mtgnList[$mtgnIdx];
            my $combinedFile = "${combineDir}/${name}_heatmap${mtgn}_${size}_combined.csv";
            for($serverIdx=0; $serverIdx<@serverList; $serverIdx++) {
                my $file = "${name}_heatmap${mtgn}_${size}_${serverIdx}.csv";
                if ($serverNumIterList[$serverIdx] > 0) {
                    my $cmd;
                    if ($serverIdx == 0) {
                        $cmd = "head -n " . ($serverNumIterList[$serverIdx]*$numFS + 1) . " $file >| $combinedFile";
                    } else {
                        $cmd = "head -n " . ($serverNumIterList[$serverIdx]*$numFS + 1) . " $file | tail -n +2 >> $combinedFile";
                    }

                    execute_command($cmd, 0);
                }
            }
        }

        my $combinedFile = "${combineDir}/${name}_exc_thr_wifi_${size}_combined.csv";
        my $sum = 0;
        for($serverIdx=0; $serverIdx<@serverList; $serverIdx++) {
            my $cmd;
            my $file = "${name}_exc_thr_wifi_${size}_${serverIdx}.csv";
            if ($serverIdx == 0) {
                $cmd = "head -n 1 $file >| $combinedFile";
                execute_command($cmd, 0);
            }

            if ($serverNumIterList[$serverIdx] > 0) {
                $cmd = "tail -n +2 $file | gawk -F \",\" '(\$2 < $serverNumIterList[$serverIdx]) {\$2 = \$2 + $sum; OFS=\",\"; print \$0}' >> $combinedFile";

                # print "CMD = $cmd\n";

                execute_command($cmd, 0);
            }
            $sum += $serverNumIterList[$serverIdx];
        }
    }
    my $t2 = time;
    print "COMBINE STEP 2, COMBINE FILES, COMPLETE, ELAPSED TIME = " . ($t2-$t1) . " sec\n";
}
################################################################################

################################################################################
# Process Files                                                                #
################################################################################
if ($processFlag) {
    my $t1 = time;
    my $numLines = $numFS * $numCombinedIterations;
    for($sizeIdx=0; $sizeIdx<@sizeList; $sizeIdx++) {
        my $size = $sizeList[$sizeIdx];
        for($mtgnIdx=0; $mtgnIdx<@mtgnList; $mtgnIdx++) {
            my $mtgn = $mtgnList[$mtgnIdx];
            my $heatmapFile = "${combineDir}/${name}_heatmap${mtgn}_${size}_combined.csv";
            my $cdfFile = "${combineDir}/cdf_${name}${mtgn}_${size}_combined.txt";
            my $fdpFile = "${combineDir}/${name}${mtgn}_${size}_combined_fdp.csv";
            my $thrFile = "${combineDir}/${name}${mtgn}_${size}_combined_thr.csv";

            my $cmd;
            if ($mtgn eq "") {

                # INDOOR
                $cmd = "tail -n +2 $heatmapFile | gawk -F \",\" '{print \$2}' | sort -rn "
                     . "| gawk 'BEGIN {pnr = 1} ((NR == 1) || (NR/pnr > 1.01)) {pnr = NR; print \$1, NR/$numLines}' >| $cdfFile";
                execute_command($cmd, 0);

                $cmd = "echo \"\" >> $cdfFile";
                execute_command($cmd, 0);

                # OUTDOOR
                $cmd = "tail -n +2 $heatmapFile | gawk -F \",\" '{print \$4}' | sort -rn "
                     . "| gawk 'BEGIN {pnr = 1} ((NR == 1) || (NR/pnr > 1.01)) {pnr = NR; print \$1, NR/$numLines}' >> $cdfFile";
                execute_command($cmd, 0);

                $cmd = "echo \"\" >> $cdfFile";
                execute_command($cmd, 0);

                # TOTAL
                $cmd = "tail -n +2 $heatmapFile | gawk -F \",\" '{printf \"%.5f\\n\", 10.0*log(exp(\$2*log(10.0)/10.0)+exp(\$4*log(10.0)/10.0))/log(10.0)}' "
                     . "| sort -rn | gawk 'BEGIN {pnr = 1} ((NR == 1) || (NR/pnr > 1.01)) {pnr = NR; print \$1, NR/$numLines}' >> $cdfFile";
                execute_command($cmd, 0);

                $cmd = "echo \"\" >> $cdfFile";
                execute_command($cmd, 0);

            } else {
                # INDOOR - for mtgn, INDOOR only
                $cmd = "tail -n +2 $heatmapFile | gawk -F \",\" '{print \$2}' | sort -rn "
                     . "| gawk 'BEGIN {pnr = 1} ((NR == 1) || (NR/pnr > 1.01)) {pnr = NR; print \$1, NR/$numLines}' >| $cdfFile";
                execute_command($cmd, 0);

                $cmd = "echo \"\" >> $cdfFile";
                execute_command($cmd, 0);
            }

            $cmd = "compute_fdp.pl -no_query -heatmap_file $heatmapFile -num_fs $numFS -iter $numCombinedIterations -fdp_file $fdpFile -thr_file $thrFile";
            execute_command($cmd, 0);
        }
    }
    my $t2 = time;
    print "PROCESS FILES COMPLETE, ELAPSED TIME = " . ($t2-$t1) . " sec\n";
}
################################################################################

exit;
#################################################################################

#################################################################################
# Routine execute_command:
#################################################################################
sub execute_command
{
    my $cmd         = $_[0];
    my $interactive = $_[1];
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

