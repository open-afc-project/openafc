/******************************************************************************************/
/**** FILE : nfa.cpp                                                                   ****/
/******************************************************************************************/

#include "nfa.h"
#include <vector>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <cmath>
#include <limits>
#include "AfcDefinitions.h"

/******************************************************************************************/
/**** CONSTRUCTOR: NFAClass::NFAClass()                                                ****/
/******************************************************************************************/
NFAClass::NFAClass()
{
    tableFile = "";
    nfaTable = (double ***)NULL;
    numxdb = -1;
    numu = -1;
    numeff = -1;
    xdbStart = quietNaN;
    uStart = quietNaN;
    effStart = quietNaN;
    xdbStep = quietNaN;
    uStep = quietNaN;
    effStep = quietNaN;
};

NFAClass::NFAClass(std::string tableFileVal) : tableFile(tableFileVal)
{
    readTable();
};
/******************************************************************************************/

/******************************************************************************************/
/**** DESTRUCTOR: NFAClass::~NFAClass()                                                ****/
/******************************************************************************************/
NFAClass::~NFAClass()
{
    if (nfaTable) {
        for (int xdbIdx = 0; xdbIdx < numxdb; ++xdbIdx) {
            for (int uIdx = 0; uIdx < numu; ++uIdx) {
                free(nfaTable[xdbIdx][uIdx]);
            }
            free(nfaTable[xdbIdx]);
        }
        free(nfaTable);
    }
};
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: NFAClass::readTable()                                                  ****/
/******************************************************************************************/
void NFAClass::readTable()
{
    string line;
    vector<string> headerList;
    vector<vector<double>> datastore;
    // data structure
    vector<double> xdB_dB;
    vector<double> u_step_count;
    vector<double> efficiency;
    vector<double> Near_field_adjustment_dB;
    std::ostringstream errStr;

    xdbStep = 1.0;
    uStep = 0.05;
    effStep = 0.05;

    double minxdb = quietNaN;
    double maxxdb = quietNaN;
    double minu = quietNaN;
    double maxu = quietNaN;
    double mineff = quietNaN;
    double maxeff = quietNaN;

    ifstream file(tableFile);
    if (!file.is_open()) {
        errStr << std::string("ERROR: Unable to open Near Field Adjustment File \"") + tableFile + std::string("\"\n");
        throw std::runtime_error(errStr.str());
    }
    int linenum = 0;

    while (getline(file, line)) {
        linenum++;
        vector<string> current_row;

        size_t last = 0;
        size_t next = 0;
        bool found_last_comma = false;

        while (!found_last_comma) {
            if ((next = line.find(',', last)) == string::npos) {
                found_last_comma = true;
            }

            current_row.push_back(line.substr(last, next - last));
            last = next + 1;
        }
        if (current_row.size() != 4) {
            errStr << std::string("ERROR: Near Field Adjustment File ") << tableFile << ":" << linenum << " INVALID DATA\n";
            throw std::runtime_error(errStr.str());
        }
        if (linenum == 1) {
        } else {
            vector<double> doubleVector(current_row.size());
            transform(current_row.begin(), current_row.end(), doubleVector.begin(), [](const std::string &val) {
                return std::stod(val);
            });
            datastore.push_back(doubleVector);

            double xdb = doubleVector[0];
            double u = doubleVector[1];
            double eff = doubleVector[2];

            if (datastore.size() == 1) {
                minxdb = xdb;
                maxxdb = xdb;
                minu = u;
                maxu = u;
                mineff = eff;
                maxeff = eff;
            } else {
                if (xdb < minxdb) {
                    minxdb = xdb;
                } else if (xdb > maxxdb) {
                    maxxdb = xdb;
                }
                if (u < minu) {
                    minu = u;
                } else if (u > maxu) {
                    maxu = u;
                }
                if (eff < mineff) {
                    mineff = eff;
                } else if (eff > maxeff) {
                    maxeff = eff;
                }
            }
        }
    }
    numxdb = (int)floor((maxxdb - minxdb) / xdbStep + 0.5) + 1;
    numu = (int)floor((maxu - minu) / uStep + 0.5) + 1;
    numeff = (int)floor((maxeff - mineff) / effStep + 0.5) + 1;

    xdbStart = minxdb;
    uStart = minu;
    effStart = mineff;

    int xdbIdx, uIdx, effIdx;

    nfaTable = (double ***)malloc(numxdb * sizeof(double **));
    for (xdbIdx = 0; xdbIdx < numxdb; ++xdbIdx) {
        nfaTable[xdbIdx] = (double **)malloc(numu * sizeof(double *));
        for (uIdx = 0; uIdx < numu; ++uIdx) {
            nfaTable[xdbIdx][uIdx] = (double *)malloc(numeff * sizeof(double));
            for (effIdx = 0; effIdx < numeff; ++effIdx) {
                nfaTable[xdbIdx][uIdx][effIdx] = quietNaN;
            }
        }
    }

    double xdb, u, eff, nfa;

    // loads parse up table data/creates data strucutre each column into 4 vectors xdB, step
    // count, efficiency, and NFA. Done for unit testing puposes to make sure all the data
    // values are present. This entire for loop can be removed.
    for (int row_ind = 0; row_ind < (int)datastore.size(); ++row_ind) {
        xdb = datastore[row_ind][0];
        u = datastore[row_ind][1];
        eff = datastore[row_ind][2];
        nfa = datastore[row_ind][3];

        xdbIdx = (int)floor(((xdb - xdbStart) / xdbStep) + 0.5);
        uIdx = (int)floor(((u - uStart) / uStep) + 0.5);
        effIdx = (int)floor(((eff - effStart) / effStep) + 0.5);
        nfaTable[xdbIdx][uIdx][effIdx] = nfa;
    }

    for (xdbIdx = 0; xdbIdx < numxdb; ++xdbIdx) {
        for (effIdx = 0; effIdx < numeff; ++effIdx) {
            bool foundDataStart = false;
            for (uIdx = numu - 1; uIdx >= 0; --uIdx) {
                if (std::isnan(nfaTable[xdbIdx][uIdx][effIdx])) {
                    if (!foundDataStart) {
                        nfaTable[xdbIdx][uIdx][effIdx] = 0.0;
                    } else {
                        xdb = xdbStart + xdbIdx * xdbStep;
                        u = uStart + uIdx * uStep;
                        eff = effStart + effIdx * effStep;
                        errStr << std::string("ERROR: Near Field "
                                              "Adjustment File ")
                               << tableFile << " does not contain data for xdb = " << xdb << ", u = " << u << ", eff = " << eff << std::endl;
                        throw std::runtime_error(errStr.str());
                    }
                } else {
                    foundDataStart = true;
                }
            }
        }
    }

    return;
};
/******************************************************************************************/

/******************************************************************************************/
/**** FUNCTION: NFAClass::computeNFA()                                                 ****/
/******************************************************************************************/
double NFAClass::computeNFA(double xdb, double u, double eff)
{
    double xdbIdxDbl = (xdb - xdbStart) / xdbStep;
    double uIdxDbl = (u - uStart) / uStep;
    double effIdxDbl = (eff - effStart) / effStep;

    if (xdbIdxDbl < 0.0) {
        xdbIdxDbl = 0.0;
    } else if (xdbIdxDbl > numxdb - 1) {
        xdbIdxDbl = (double)numxdb - 1;
    }

    if (uIdxDbl < 0.0) {
        uIdxDbl = 0.0;
    } else if (uIdxDbl > numu - 1) {
        uIdxDbl = (double)numu - 1;
    }

    if (effIdxDbl < 0.0) {
        effIdxDbl = 0.0;
    } else if (effIdxDbl > numeff - 1) {
        effIdxDbl = (double)numeff - 1;
    }

    int xdbIdx0 = (int)floor(xdbIdxDbl);
    if (xdbIdx0 == numxdb - 1) {
        xdbIdx0 = numxdb - 2;
    }

    int uIdx0 = (int)floor(uIdxDbl);
    if (uIdx0 == numu - 1) {
        uIdx0 = numu - 2;
    }

    int effIdx0 = (int)floor(effIdxDbl);
    if (effIdx0 == numeff - 1) {
        effIdx0 = numeff - 2;
    }

    double F000 = nfaTable[xdbIdx0][uIdx0][effIdx0];
    double F001 = nfaTable[xdbIdx0][uIdx0][effIdx0 + 1];
    double F010 = nfaTable[xdbIdx0][uIdx0 + 1][effIdx0];
    double F011 = nfaTable[xdbIdx0][uIdx0 + 1][effIdx0 + 1];
    double F100 = nfaTable[xdbIdx0 + 1][uIdx0][effIdx0];
    double F101 = nfaTable[xdbIdx0 + 1][uIdx0][effIdx0 + 1];
    double F110 = nfaTable[xdbIdx0 + 1][uIdx0 + 1][effIdx0];
    double F111 = nfaTable[xdbIdx0 + 1][uIdx0 + 1][effIdx0 + 1];

    double nfa = F000 * (xdbIdx0 + 1 - xdbIdxDbl) * (uIdx0 + 1 - uIdxDbl) * (effIdx0 + 1 - effIdxDbl) +
        F001 * (xdbIdx0 + 1 - xdbIdxDbl) * (uIdx0 + 1 - uIdxDbl) * (effIdxDbl - effIdx0) +
        F010 * (xdbIdx0 + 1 - xdbIdxDbl) * (uIdxDbl - uIdx0) * (effIdx0 + 1 - effIdxDbl) +
        F011 * (xdbIdx0 + 1 - xdbIdxDbl) * (uIdxDbl - uIdx0) * (effIdxDbl - effIdx0) +
        F100 * (xdbIdxDbl - xdbIdx0) * (uIdx0 + 1 - uIdxDbl) * (effIdx0 + 1 - effIdxDbl) +
        F101 * (xdbIdxDbl - xdbIdx0) * (uIdx0 + 1 - uIdxDbl) * (effIdxDbl - effIdx0) +
        F110 * (xdbIdxDbl - xdbIdx0) * (uIdxDbl - uIdx0) * (effIdx0 + 1 - effIdxDbl) +
        F111 * (xdbIdxDbl - xdbIdx0) * (uIdxDbl - uIdx0) * (effIdxDbl - effIdx0);

    return (nfa);
}
/******************************************************************************************/
