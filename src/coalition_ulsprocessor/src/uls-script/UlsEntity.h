#ifndef ULS_ENTITY_H
#define ULS_ENTITY_H

class UlsEntity
{
	public:
		long long systemId;
		char ulsFilenumber[15];
		char ebfNumber[31];
		char callsign[11];
		char entityType[3];
		char licenseeId[10];
		char entityName[201];
		char firstName[21];
		char mi[2];
		char lastName[21];
		char suffix[4];
		char phone[11];
		char fax[11];
		char email[51];
		char street[61];
		char city[21];
		char state[3];
		char zip[10];
		char pobox[21];
		char attnLine[36];
		char sgin[4];
		char frn[11];
		char applicantTypeCode[2];
		char applicantTypeCodeOther[41];
		char statusCode[1];
		char statusDate[14];

		enum Parameter {
			MinEntityParameter = 0x05000000,
			EntityUlsFilenumber = 0x05000001,
			EntityEbfNumber = 0x05000002,
			EntityCallSign = 0x05000003,
			EntityEntityType = 0x05000004,
			EntityLicenseeId = 0x05000005,
			EntityEntityName = 0x05000006,
			EntityFirstName = 0x05000007,
			EntityMiddleInit = 0x05000008,
			EntityLastName = 0x05000009,
			EntitySuffix = 0x0500000a,
			EntityPhoneNumber = 0x0500000b,
			EntityFaxNumber = 0x0500000c,
			EntityEmailAddress = 0x0500000d,
			EntityStreetAddress = 0x0500000e,
			EntityState = 0x0500000f,
			EntityZipCode = 0x05000010,
			EntityPoBox = 0x05000011,
			EntityAttnLine = 0x05000012,
			EntitySGIN = 0x05000013,
			EntityFRN = 0x05000014,
			EntityApplicantTypeCode = 0x05000015,
			EntityApplicantTypeCodeOther = 0x05000016,
			EntityStatusCode = 0x05000017,
			EntityStatusDate = 0x05000018,
			MaxEntityParameter = 0x05000019,
		};
};

#endif
