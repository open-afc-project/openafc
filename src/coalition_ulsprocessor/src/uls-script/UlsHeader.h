#ifndef ULS_HEADER_H
#define ULS_HEADER_H

#include <QDate>

class UlsHeader
{
    public:
        long long systemId;
        char ulsFilenumber[15];
        char ebfNumber[31];
        char callsign[11];
        char licenseStatus;
        char radioServiceCode[3];
        char grantDate[14];
        char expiredDate[14];
        char cancellationDate[14];
        char eligibilityNum[11];
        char reserved1;
        char alien;
        char alienGovernment;
        char alienCorporation;
        char alienOfficer;
        char alienControl;
        char revoked;
        char convicted;
        char adjudged;
        char reserved2;
        char commonCarrier;
        char nonCommonCarrier;
        char privateCarrier;
        char fixed;
        char mobile;
        char radiolocation;
        char satellite;
        char developmental;
        char interconnected;
        char certifierFirstName[21];
        char certifierMiddleInitial;
        char certifierLastName[21];
        char certifierSuffix[3];
        char certifierTitle[41];
        char female;
        char blackAfAmerican;
        char nativeAmerican;
        char hawaiian;
        char asian;
        char white;
        char effectiveDate[14];
        char lastActionDate[14];
        int auctionId;
        char broadcastServiceStatus;
        char bandManager;
        char broadcastType;
        char alienRuling;
        char licenseeNameChange;

        enum Parameter {
            MinHeaderParameter = 0x06000000,
            HeaderSystemId = 0x06000001,
            HeaderUlsFileNumber = 0x06000002,
            HeaderEbfNumber = 0x06000003,
            HeaderCallsign = 0x06000004,
            HeaderLicenseStatus = 0x06000005,
            HeaderRadioServiceCode = 0x06000006,
            HeaderGrantDate = 0x06000007,
            HeaderExpiredDate = 0x06000008,
            HeaderCancellationDate = 0x06000009,
            HeaderEligibilityNumber = 0x0600000a,
            HeaderReserved1 = 0x0600000b,
            HeaderAlien = 0x0600000c,
            HeaderAlienGovernment = 0x0600000d,
            HeaderAlienCorporation = 0x0600000e,
            HeaderAlienOfficer = 0x0600000f,
            HeaderAlienControl = 0x06000010,
            HeaderRevoked = 0x06000011,
            HeaderConvicted = 0x06000012,
            HeaderAdjudged = 0x06000013,
            HeaderReserved2 = 0x06000014,
            HeaderCommonCarrier = 0x06000015,
            HeaderNonCommonCarrier = 0x06000016,
            HeaderPrivateCarrier = 0x06000017,
            HeaderFixed = 0x06000018,
            HeaderMobile = 0x06000019,
            HeaderRadiolocation = 0x0600001a,
            HeaderSatellite = 0x0600001b,
            HeaderDevelopmental = 0x0600001c,
            HeaderInterconnected = 0x0600001d,
            HeaderCertifierFirstName = 0x0600001e,
            HeaderCertifierMiddleInitial = 0x0600001f,
            HeaderCertifierLastName = 0x06000020,
            HeaderCertifierSuffix = 0x06000021,
            HeaderCertifierTitle = 0x06000022,
            HeaderFemale = 0x06000023,
            HeaderBlackAfAmerican = 0x06000024,
            HeaderNativeAmerican = 0x06000025,
            HeaderHawaiian = 0x06000026,
            HeaderAsian = 0x06000027,
            HeaderWhite = 0x06000028,
            HeaderEffectiveDate = 0x06000029,
            HeaderLastActionDate = 0x0600002a,
            HeaderAuctionId = 0x0600002b,
            HeaderBroadcastServiceStatus = 0x0600002c,
            HeaderBandManager = 0x0600002d,
            HeaderBroadcastType = 0x0600002e,
            HeaderAlienRuling = 0x0600002f,
            HeaderLicenseeNameChange = 0x06000030,
            MaxHeaderParameter = 0x06000031,
        };
};

#endif
