#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <getopt.h>
#include <fenv.h>
#include <iostream>
#include <limits.h>

#include "data_set.h"

void set_options(int argc, char **argv, std::string& templateFile);

/******************************************************************************************/
int main(int argc, char **argv)
{
    char hostname[HOST_NAME_MAX];
    char username[LOGIN_NAME_MAX];
    gethostname(hostname, HOST_NAME_MAX);
    getlogin_r(username, LOGIN_NAME_MAX);

    std::cout << "Running on " << hostname << " USER = " << username << std::endl;

    char *tstr;

    time_t t1 = time(NULL);
    tstr = strdup(ctime(&t1));
    strtok(tstr, "\n");
    std::cout << tstr << " : Beginning ANALYSIS." << std::endl;
    free(tstr);

    // feenableexcept(FE_INVALID | FE_OVERFLOW);

    std::string templateFile;
    set_options(argc, argv, templateFile);
    std::cout << "TEMPLATE FILE = " << templateFile << std::endl;

    DataSetClass *dataSet = new DataSetClass();

    try {
        dataSet->parameterTemplate.readFile(templateFile.c_str());
        dataSet->parameterTemplate.print(stdout);

        if (dataSet->parameterTemplate.seed == 0) {
            struct timespec tseed;
            clock_gettime(CLOCK_REALTIME, &tseed);
            unsigned int seed = (unsigned int) ((((unsigned int) tseed.tv_sec)*1000000000 + tseed.tv_nsec)&0xFFFFFFFF);
            std::cout << "SEED GENERATED FROM clock_gettime() = " << seed << std::endl;
            srand(seed);
        } else {
            srand(dataSet->parameterTemplate.seed);
        }
        dataSet->run();
    }
    catch (std::exception &e) {
        std::cout << e.what() << std::endl;
    }

    delete dataSet;

    time_t t2 = time(NULL);
    tstr = strdup(ctime(&t2));
    strtok(tstr, "\n");
    std::cout << tstr << " : Completed ANALYSIS." << std::endl;
    free(tstr);

    int elapsedTime = (int) (t2-t1);

    int et = elapsedTime;
    int elapsedTimeSec = et % 60;
    et = et / 60;
    int elapsedTimeMin = et % 60;
    et = et / 60;
    int elapsedTimeHour = et % 24;
    et = et / 24;
    int elapsedTimeDay = et;

    std::cout << "Elapsed time = " << (t2-t1) << " sec = "
              << elapsedTimeDay  << " days "
              << elapsedTimeHour << " hours "
              << elapsedTimeMin  << " min "
              << elapsedTimeSec  << " sec." << std::endl;

    return(1);
}
/******************************************************************************************/

/******************************************************************************************/
/*** Set all variables defined by command line options.                              ******/
void set_options(int argc, char **argv, std::string& templateFile)
{
    static const char  *help_msg[] = {
    " -templ     --file    parameter template file",
    " -h         --help    print this help message",
    " ",
    0};
    static const char  *usage[] = {
            " [ -option value] [ -h ]",
    0};
    char *name = *argv;
    const char **p = help_msg;
    const char **u = usage;
    while ( --argc > 0 ) {
        argv++;
             if (strcmp(*argv,"-templ")    ==0) { templateFile = std::string(*++argv); argc--; }

        else if (strcmp(*argv,"-h")==0)
        {   fprintf(stdout, "\n\n");
            fprintf(stdout, "usage:\n%s", name);
            while (*u) fprintf(stdout, "%s\n", *u++);
            fprintf(stdout, "\n");
            while (*p) fprintf(stdout, "%s\n", *p++);
            exit(1); }
        else
        {   fprintf(stderr, "\n\n%s Invalid Option: %s \n", name, *argv);
            fprintf(stderr, "\n\n");
            fprintf(stderr, "usage:\n%s", name);
            while (*u) fprintf(stderr, "%s\n", *u++);
            fprintf(stderr, "\n");
            exit(1); }
    }
}
/******************************************************************************************/

