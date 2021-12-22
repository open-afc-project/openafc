#ifndef ULS_SEGMENT_H
#define ULS_SEGMENT_H

class UlsSegment {
public:
    long long systemId;
	char callsign[11];
    int pathNumber;
    int txLocationId;
    int txAntennaId;;
    int rxLocationId;
    int rxAntennaId;
    int segmentNumber;
    double segmentLength;
};

#endif
