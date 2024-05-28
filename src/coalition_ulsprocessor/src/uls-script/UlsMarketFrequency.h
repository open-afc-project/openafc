#ifndef ULS_MARKET_FREQUENCY_H
#define ULS_MARKET_FREQUENCY_H

class UlsMarketFrequency
{
    public:
        long long int systemId;
        char callsign[11];
        char partitionSeq[7];
        double lowerFreq;
        double upperFreq;
};

#endif