#ifndef ULS_CONTROL_POINT_H
#define ULS_CONTROL_POINT_H

class UlsControlPoint
{
	public:
		long long systemId;
		char ulsFilenumber[15];
		char ebfNumber[31];
		char callsign[11];
		char controlPointActionPerformed;
		int controlPointNumber;
		char controlPointAddress[81];
		char controlPointCity[21];
		char controlPointState[3];
		char controlPointPhone[11];
		char controlPointCounty[61];
		char controlPointStatus[1];
		char controlPointStatusDate[14];
};

#endif
