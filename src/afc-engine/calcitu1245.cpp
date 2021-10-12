#include "calcitu1245.h"

using namespace std;

namespace calcItu1245{

 double mymin(const double &a, const double &b)
{ if(a<b)
    return a;
  else
    return b;
}

double mymax(const double &a, const double &b)
{ if(a>b)
    return a;
  else
    return b;
}

double CalcITU1245(const double &angleDeg, const double &maxGain){

    double cAngleDeg = angleDeg;

    while(cAngleDeg >= 360.0){
        cAngleDeg = cAngleDeg - 360.0;
    }

    if(cAngleDeg > 180.0){
        double gt180 = cAngleDeg - 180;
        cAngleDeg = cAngleDeg - gt180*2.0;
    }

    double Dlambda = pow(10, ((maxGain - 7.7)/20));
    double g1 = 2.0 + 15.0 * log10(Dlambda);
    double psiM = 20.0 * (1.0 / Dlambda) * pow((maxGain - g1), 0.5);
    double psiR = 12.02 * pow(Dlambda, -0.6);
    double rv;

    //qDebug() << Dlambda;

    if(Dlambda > 100.0)
    {
        if(cAngleDeg >= 0.0 && cAngleDeg < psiM){
            rv = maxGain - 2.5 * pow(10, -3.0) * pow((Dlambda * cAngleDeg), 2.0);
        }
        else if (cAngleDeg >= psiM && cAngleDeg < mymax(psiM, psiR)) {
            rv = g1;
        }
        else if (cAngleDeg >= mymax(psiM, psiR) && cAngleDeg < 48.0) {
            rv = 29.0 - 25.0 * log10(cAngleDeg);
        }
        else if (cAngleDeg >= 48.0 && cAngleDeg <= 180.0) {
            rv = -13.0;
        }
    }
    else if (Dlambda <= 100.0) {
        //qDebug() << cAngleDeg << psiM;
        if(cAngleDeg >= 0.0 && cAngleDeg < psiM){
            rv = maxGain - 2.5 * pow(10, -3.0) * pow((Dlambda * cAngleDeg), 2.0);
        }
        else if (cAngleDeg >= psiM && cAngleDeg < 48.0) {
            rv = 39.0 - 5.0 * log10(Dlambda) - 25.0 * log10(cAngleDeg);
            //qDebug() << "rv = 39 - 5 * log10(" << Dlambda << ") - 25 * log10(" << cAngleDeg << ") = "<< rv;
        }
        else if (cAngleDeg >= 48.0 && cAngleDeg <= 180.0) {
            rv = -3.0 - 5.0 * log10(Dlambda);
        }

    }
    return rv;

}

double CalcITU1245psiM(const double &maxGain){
    double Dlambda = pow(10, ((maxGain - 7.7)/20));
    double g1 = 2.0 + 15.0 * log10(Dlambda);
    double psiM = 20.0 * (1.0 / Dlambda) * pow((maxGain - g1), 0.5);

    return(psiM);
}

double CalcFCCPattern(const double &angleDeg, const double &maxGain){

    double cAngleDeg = angleDeg;

    while(cAngleDeg >= 360.0){
        cAngleDeg = cAngleDeg - 360.0;
    }

    if(cAngleDeg > 180.0){
        double gt180 = cAngleDeg - 180;
        cAngleDeg = cAngleDeg - gt180*2.0;
    }

    double rv;

    if(cAngleDeg < 5.0){
        rv = CalcITU1245(cAngleDeg, maxGain);
    }

    else if(cAngleDeg >= 5.0 && cAngleDeg < 10.0){
        rv =  maxGain - 25.0;
    }
    else if(cAngleDeg >= 10.0 && cAngleDeg < 15.0){
        rv =  maxGain - 29.0;
    }
    else if(cAngleDeg >= 15.0 && cAngleDeg < 20.0){
        rv =  maxGain - 33.0;
    }
    else if(cAngleDeg >= 20.0 && cAngleDeg < 30.0){
        rv =  maxGain - 36.0;
    }
    else if(cAngleDeg >= 30.0 && cAngleDeg < 100.0){
        rv =  maxGain - 42.0;
    }
    else if(cAngleDeg >= 100.0 && cAngleDeg <= 180.0){
        rv =  maxGain - 55.0;
    }
    else{
            rv =  maxGain - 55.0;
    }
    return rv;

}

double CalcETSIClass4(const double &angleDeg, const double &maxGain){

    double cAngleDeg = angleDeg;

    while(cAngleDeg >= 360.0){
        cAngleDeg = cAngleDeg - 360.0;
    }

    if(cAngleDeg > 180.0){
        double gt180 = cAngleDeg - 180;
        cAngleDeg = cAngleDeg - gt180*2.0;
    }

    double rv;

    if(cAngleDeg < 5.0){
        rv = CalcITU1245(cAngleDeg, maxGain);
    }
    else if(cAngleDeg <10.0){

        double slope = (16.0-5.0)/(5.0-10.0);

        rv = slope * (cAngleDeg - 5.0) + 16.0;
    }
    else if(cAngleDeg <20.0){
        double slope = (5.0- -7.0)/(10.0-20.0);
         rv = slope * (cAngleDeg - 10.0) + 5.0;
    }
    else if(cAngleDeg < 50.0){
        double slope = (-7.0 - -18.0)/(20.0 - 50.0);
         rv = slope * (cAngleDeg - 20.0) + -7.0;
    }
    else if(cAngleDeg < 70.0){
        double slope = (-18.0 - -20.0)/(50.0 - 70.0);
         rv = slope * (cAngleDeg - 50.0) + -18.0;
    }
    else if(cAngleDeg < 85.0){
        double slope = (-20.0 - -24.0)/(70.0 - 85.0);
         rv = slope * (cAngleDeg - 70.0) + -20.0;
    }
    else if(cAngleDeg < 105.0){
        double slope = (-24.0 - -30.0)/(85.0 - 105.0);
         rv = slope * (cAngleDeg - 85.0) + -24.0;
    }
    else{
        rv = -30.0;
    }

    return rv;
}
}

