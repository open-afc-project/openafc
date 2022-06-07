#ifndef CCONST_H
#define CCONST_H

class StrTypeClass;

class CConst
{
public:
	static const double c;	// speed of light in m/s
	static const double u0;	// Permeability of free space H/m
	static const double e0;	// Permittivity of free space F/m
	static const double logTable[];	// logTable[i] = LOG10(1+i)   i = [0,8]

	static const double earthRadius;	// Radius of earth in m
	static const double geoRadius;	// Radius of geosynchronous orbit
	static const double boltzmannConstant;	// Boltzman's constant
	static const double T0;	// Room temperature for noise figure calculations
	static const double atmPressurehPa;	// Atmospheric pressure value used in P.452



	/**************************************************************************************/
	/**** BuildingType                                                                 ****/
	/**************************************************************************************/
	enum BuildingTypeEnum
	{
		noBuildingType,		// outdoor
		traditionalBuildingType,
		thermallyEfficientBuildingType
	};
	/**************************************************************************************/

	/**************************************************************************************/
	/**** PropEnv                                                                      ****/
	/**************************************************************************************/
	enum PropEnvEnum
	{
		unknownPropEnv,		// 'X'
		urbanPropEnv,		// 'U'
		suburbanPropEnv,		// 'S'
		ruralPropEnv,		// 'R'
		barrenPropEnv		// 'B'
	};
	/**************************************************************************************/

	/**************************************************************************************/
	/**** NLCDLandCat                                                                  ****/
	/**************************************************************************************/
	enum NLCDLandCatEnum
	{
		deciduousTreesNLCDLandCat,
		coniferousTreesNLCDLandCat,
		highCropFieldsNLCDLandCat,
		noClutterNLCDLandCat,
		villageCenterNLCDLandCat,

		unknownNLCDLandCat
	};
	/**************************************************************************************/

	/**************************************************************************************/
	/**** UserType                                                                     ****/
	/**************************************************************************************/
	enum UserTypeEnum
	{
		unknownUserType,
		corporateUserType,
		publicUserType,
		homeUserType
	};
	/**************************************************************************************/

	/**************************************************************************************/
	/**** AntennaModel                                                                 ****/
	/**************************************************************************************/
	enum AntennaModelEnum
	{
		idealOmniAntennaModel,

		indoorUTAverageAntennaModel,
		outdoorClientAntennaModel,
		outdoorDirPtToPtAntennaModel,
		outdoorOmniPtToMptAntennaModel,
		outdoorOmniRLANAntennaModel
	};
	/**************************************************************************************/

	/**************************************************************************************/
	/**** AntennaType                                                                  ****/
	/**************************************************************************************/
	enum AntennaTypeEnum
	{
		antennaOmni,
		antennaLUT_H,
		antennaLUT_V,
		antennaLUT_Boresight,
		antennaLUT
	};
	/**************************************************************************************/

	/**************************************************************************************/
	/**** ULSType                                                             ****/
	/**************************************************************************************/
	enum ULSTypeEnum
	{
		ESULSType,
		AMTULSType
	};
	/**************************************************************************************/

	/**************************************************************************************/
	/**** ULSAntennaType                                                               ****/
	/**************************************************************************************/
	enum ULSAntennaTypeEnum
	{
		OmniAntennaType,
		F1336OmniAntennaType,
		F1245AntennaType,
		F699AntennaType,
		R2AIP07AntennaType,
		LUTAntennaType
	};
	/**************************************************************************************/

	/**************************************************************************************/
	/**** PathLossModel                                                                ****/
	/**************************************************************************************/
	enum PathLossModelEnum
	{
		unknownPathLossModel,
		ITMBldgPathLossModel,
		CoalitionOpt6PathLossModel,
		FCC6GHzReportAndOrderPathLossModel,
		CustomPathLossModel,
		FSPLPathLossModel
	};
	/**************************************************************************************/

	/**************************************************************************************/
	/**** Global Parameters                                                            ****/
	/**************************************************************************************/
	enum GParam
	{
		antenna_num_interp_pts = 361
	};
	/**************************************************************************************/

	/**************************************************************************************/
	/**** SimulationEnum                                                               ****/
	/**************************************************************************************/
	enum SimulationEnum
	{
		FixedSimulation,
		MobileSimulation,
		RLANSensingSimulation,
		showFSPwrAtRLANSimulation,
		FSToFSSimulation
	};
	/**************************************************************************************/

	/**************************************************************************************/
	/**** LengthUnit                                                                   ****/
	/**************************************************************************************/
	enum LengthUnitEnum
	{
		mmLengthUnit,
		cmLengthUnit,
		mLengthUnit,
		KmLengthUnit,
		milLengthUnit,
		inLengthUnit,
		ftLengthUnit
	};
	/**************************************************************************************/

	/**************************************************************************************/
	/**** AngleUnit                                                                    ****/
	/**************************************************************************************/
	enum AngleUnitEnum
	{
		radianAngleUnit = 0,
		degreeAngleUnit
	};
	/**************************************************************************************/

	/**************************************************************************************/
	/**** PSDDBUnit                                                                    ****/
	/**************************************************************************************/
	enum PSDDBUnitEnum
	{
		WPerHzPSDDBUnit = 0,
		WPerMHzPSDDBUnit,
		WPer4KHzPSDDBUnit
	};
	/**************************************************************************************/

	/**************************************************************************************/
	/**** LidarFormatEnum                                                              ****/
	/**************************************************************************************/
	enum LidarFormatEnum
	{
		fromVectorLidarFormat,	// building data comes from vector data
		fromRasterLidarFormat	// building data comes from raster data
	};
	/**************************************************************************************/

	/**************************************************************************************/
	/**** HeightSource                                                                 ****/
	/**************************************************************************************/
	enum HeightSourceEnum
	{
		unknownHeightSource,
		globalHeightSource,
		depHeightSource,
		srtmHeightSource,
		lidarHeightSource
	};
	/**************************************************************************************/

	/**************************************************************************************/
	/**** HeightType                                                                   ****/
	/**************************************************************************************/
	enum HeightTypeEnum
	{
		AMSLHeightType,
		AGLHeightType
	};
	/**************************************************************************************/

	/**************************************************************************************/
	/**** LOSOption                                                                    ****/
	/**************************************************************************************/
	enum LOSOptionEnum
	{
		UnknownLOSOption,
		BldgDataLOSOption,
		BldgDataReqTxLOSOption,
		BldgDataReqRxLOSOption,
		BldgDataReqTxRxLOSOption,
		ForceLOSLOSOption,
		ForceNLOSLOSOption
	};
	/**************************************************************************************/

	/**************************************************************************************/
	/**** Winner2UnknownLOSMethod                                                      ****/
	/**************************************************************************************/
	enum Winner2UnknownLOSMethodEnum
	{
		PLOSCombineWinner2UnknownLOSMethod,
		PLOSThresholdWinner2UnknownLOSMethod
	};
	/**************************************************************************************/

	/**************************************************************************************/
	/**** SpectralAlgorithm                                                            ****/
	/**************************************************************************************/
	enum SpectralAlgorithmEnum
	{
		pwrSpectralAlgorithm,
		psdSpectralAlgorithm
	};
	/**************************************************************************************/

	/**************************************************************************************/
	/**** ITMClutterMethod                                                             ****/
	/**************************************************************************************/
	enum ITMClutterMethodEnum
	{
		ForceTrueITMClutterMethod,
		ForceFalseITMClutterMethod,
		BldgDataITMCLutterMethod
	};
	/**************************************************************************************/

	/**************************************************************************************/
	/**** ResponseCode                                                                 ****/
	/**************************************************************************************/
	enum ResponseCodeEnum
	{
		generalFailureResponseCode = -1,
		successResponseCode = 0,
		versionNotSupportedResponseCode = 100,
		deviceDisallowedResponseCode = 101,
		missingParamResponseCode = 102,
		invalidValueResponseCode = 103,
		unexpectedParamResponseCode = 106,
		unsupportedSpectrumResponseCode = 300
	};
	/**************************************************************************************/

	/**************************************************************************************/
	/**** ScanPointBelowGroundMethod                                                   ****/
	/**************************************************************************************/
	enum ScanPointBelowGroundMethodEnum
	{
		DiscardScanPointBelowGroundMethod,
		TruncateScanPointBelowGroundMethod
	};
	/**************************************************************************************/

	/**************************************************************************************/
	/**** ScanRegionMethod                                                             ****/
	/**************************************************************************************/
	enum ScanRegionMethodEnum
	{
		xyAlignRegionNorthEastScanRegionMethod,
		xyAlignRegionMajorMinorScanRegionMethod,
		latLonAlignGridScanRegionMethod
	};
	/**************************************************************************************/

	static const StrTypeClass strULSAntennaTypeList[];
	static const StrTypeClass strPropEnvList[];
	static const StrTypeClass strPathLossModelList[];
	static const StrTypeClass strLOSOptionList[];
	static const StrTypeClass strITMClutterMethodList[];
	static const StrTypeClass strHeightSourceList[];
	static const StrTypeClass strSpectralAlgorithmList[];
};

#endif
