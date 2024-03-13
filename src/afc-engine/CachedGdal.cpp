/*
 * Copyright (C) 2022 Broadcom. All rights reserved.
 * The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
 * that owns the software below.
 * This work is licensed under the OpenAFC Project License, a copy of which is
 * included with this software program.
 */
#include "CachedGdal.h"
#include <assert.h>
#include <boost/filesystem.hpp>
#include <fnmatch.h>
#include <afclogging/Logging.h>
#include <sstream>
#include <stdexcept>

namespace {
	// Logger for all instances of class
	LOGGER_DEFINE_GLOBAL(logger, "CachedGdal")

} // end namespace

  ///////////////////////////////////////////////////////////////////////////////
  // CachedGdalBase::GdalInfo
  ///////////////////////////////////////////////////////////////////////////////

CachedGdalBase::GdalInfo::GdalInfo(const GdalDatasetHolder *gdalDataset, int minBands,
	const boost::optional<std::function<void(GdalTransform *)>> &transformationModifier)
	: baseName(boost::filesystem::path(gdalDataset->fullFileName).filename().string()),
	transformation(gdalDataset->gdalDataset, baseName), numBands(minBands)
{
	if (transformationModifier) {
		transformationModifier.get()(&transformation);
	}
	boundRect = transformation.makeBoundRect();
	std::ostringstream errStr;
	boost::system::error_code systemErr;
	if (gdalDataset->gdalDataset->GetRasterCount() < minBands) {
		errStr << "ERROR: CachedGdalBase::GdalData::GdalData(): GDAL data file '"
			<< baseName << "' has only " << gdalDataset->gdalDataset->GetRasterCount() <<
			" bands, whereas at least " << minBands << "is expected";
		throw std::runtime_error(errStr.str());
	}
	for (int i = 0; i < minBands; ++i) {
		noDataValues.push_back(
			gdalDataset->gdalDataset->GetRasterBand(i + 1)->GetNoDataValue());
	}
}

///////////////////////////////////////////////////////////////////////////////
// CachedGdalBase::TileKey
///////////////////////////////////////////////////////////////////////////////

CachedGdalBase::TileKey::TileKey(int band_, int latOffset_, int lonOffset_,
	const std::string &baseName_)
	: band(band_), latOffset(latOffset_), lonOffset(lonOffset_), baseName(baseName_)
{}

CachedGdalBase::TileKey::TileKey()
	: band(0), latOffset(0), lonOffset(0), baseName()
{}

bool CachedGdalBase::TileKey::operator <(const TileKey &other) const
{
	if (band != other.band) return band < other.band;
	if (latOffset != other.latOffset) return latOffset < other.latOffset;
	if (lonOffset != other.lonOffset) return lonOffset < other.lonOffset;
	return baseName < other.baseName;
}

///////////////////////////////////////////////////////////////////////////////
// CachedGdalBase::TileInfo
///////////////////////////////////////////////////////////////////////////////

CachedGdalBase::TileInfo::TileInfo(CachedGdalBase *cachedGdal_,
	const GdalTransform &transformation_, const GdalInfo *gdalInfo_)
	: cachedGdal(cachedGdal_), transformation(transformation_),
	boundRect(transformation_.makeBoundRect()), gdalInfo(gdalInfo_),
	tileVector(cachedGdal_->createTileVector(transformation_.latSize, transformation_.lonSize),
	[cachedGdal_](void *p) {cachedGdal_->deleteTileVector(p);})
{}

CachedGdalBase::TileInfo::TileInfo() : cachedGdal(nullptr), gdalInfo(nullptr)
{}
		
///////////////////////////////////////////////////////////////////////////////
// CachedGdalBase::GdalDatasetHolder
///////////////////////////////////////////////////////////////////////////////

CachedGdalBase::GdalDatasetHolder::GdalDatasetHolder(const std::string &fullFileName_)
	: fullFileName(fullFileName_)
{
	std::ostringstream errStr;
	boost::system::error_code systemErr;
	if (!boost::filesystem::is_regular_file(fullFileName, systemErr)) {
		errStr << "ERROR: CachedGdalBase::GdalDatasetHolder::GdalDatasetHolder(): "
			"GDAL data file '" << fullFileName << "' not found";
		throw std::runtime_error(errStr.str());
	}
	gdalDataset = static_cast<GDALDataset*>(GDALOpen(fullFileName.c_str(), GA_ReadOnly));
	if (!gdalDataset) {
		errStr << "ERROR: CachedGdalBase::GdalDatasetHolder::GdalDatasetHolder(): "
			"Error opening GDAL data file '" << fullFileName << "': " << CPLGetLastErrorMsg();
		throw std::runtime_error(errStr.str());
	}
	LOGGER_DEBUG(logger) << "Opened GDAL file '" << fullFileName << "'";
}

CachedGdalBase::GdalDatasetHolder::~GdalDatasetHolder()
{
	GDALClose(gdalDataset);
}

///////////////////////////////////////////////////////////////////////////////
// CachedGdalBase::PixelInfo
///////////////////////////////////////////////////////////////////////////////

CachedGdalBase::PixelInfo::PixelInfo(const std::string &baseName_, int row_, int column_)
	: baseName(baseName_), row(row_), column(column_)
{}

CachedGdalBase::PixelInfo::PixelInfo() : baseName(""), row(-1), column(-1)
{}

///////////////////////////////////////////////////////////////////////////////
// CachedGdalBase
///////////////////////////////////////////////////////////////////////////////

CachedGdalBase::CachedGdalBase(std::string fileOrDir,  const std::string &dsName,
	std::unique_ptr<GdalNameMapperBase> nameMapper, int numBands, int maxTileSize,
	int cacheSize, GDALDataType pixelType)
	: _fileOrDir(fileOrDir), _dsName(dsName), _nameMapper(std::move(nameMapper)),
	_numBands(numBands), _pixelType(pixelType), _maxTileSize(maxTileSize),
	_tileCache(cacheSize), _gdalDsCache(GDAL_CACHE_SIZE), _recentGdalInfo(nullptr),
	_allSeen(false)
{
	GDALAllRegister();
}
	
void CachedGdalBase::initialize()
{
	LOGGER_INFO(logger) << "Initializing access to '" << _fileOrDir <<
		(isMonolithic() ? "' GDAL file " : "' GDAL file directory ") <<
		"containing " << (dsName().empty() ? "?some?" : dsName()) <<
		" data. Assumed pixel data type is " << GDALGetDataTypeName(_pixelType) <<
		", number of bands is " << _numBands;
	boost::system::error_code systemErr;
	std::ostringstream errStr;
	if (isMonolithic()) {
		if (!boost::filesystem::is_regular_file(_fileOrDir, systemErr)) {
			errStr << "ERROR: CachedGdalBase::initialize(): "
				"GDAL file '" << _fileOrDir << "' not found";
			throw std::runtime_error(errStr.str());
		}
		std::string baseName = boost::filesystem::path(_fileOrDir).filename().string();
		addGdalInfo(baseName, getGdalDatasetHolder(_fileOrDir));
		_allSeen = true;
	} else {
		if (!boost::filesystem::is_directory(_fileOrDir, systemErr)) {
			errStr << "ERROR: CachedGdalBase::initialize(): "
				"GDAL data directory '" << _fileOrDir <<
				"' not found or is not a directory";
			throw std::runtime_error(errStr.str());
		}
		if (!forEachGdalInfo([](const GdalInfo& gdalInfo) {return true; })) {
			errStr << "ERROR: CachedGdalBase::initialize(): "
				"GDAL data directory '" << _fileOrDir <<
				"' does not contain files matching fnmatch pattern '" <<
				_nameMapper->fnmatchPattern() << "'";
			throw std::runtime_error(errStr.str());
		}
	}
}

void CachedGdalBase::cleanup()
{
	_tileCache.clear();
}

const std::string &CachedGdalBase::dsName() const
{
	return _dsName;
}

void CachedGdalBase::setTransformationModifier(
	std::function<void(GdalTransform *)> modifier)
{
	_transformationModifier = modifier;
	rereadGdal();
}

bool CachedGdalBase::isMonolithic() const
{
	return !_nameMapper.get();
}

void CachedGdalBase::rereadGdal()
{
	_tileCache.clear();
	std::string anyBaseName = _gdalInfos.begin()->first;
	_gdalInfos.clear();
	addGdalInfo(anyBaseName, getGdalDatasetHolder(anyBaseName));
	if (!isMonolithic()) {
		_allSeen = false;
	}
}

bool CachedGdalBase::forEachGdalInfo(
	const std::function<bool(const CachedGdalBase::GdalInfo& gdalInfo)>& op)
{
	// First iterating over previously seen (multifile and monolithic case)
	for (const auto& it: _gdalInfos) {
		const GdalInfo* gdalInfo = it.second.get();
		if (gdalInfo && op(*gdalInfo)) {
			return true;
		}
	}
	// Are there any not yet seen GDAL files?
	if (_allSeen) {
		return false;
	}
	// Iterating over not yet seen GDAL files
	std::string fnmatch_pattern = _nameMapper->fnmatchPattern();
	for (boost::filesystem::directory_iterator di(_fileOrDir);
		di != boost::filesystem::directory_iterator(); ++di)
	{
		std::string baseName = di->path().filename().native();
		// Skipping seen, nonmatching and nonfiles
		if ((_gdalInfos.find(baseName) != _gdalInfos.end()) ||
			(fnmatch(fnmatch_pattern.c_str(), baseName.c_str(), 0) == FNM_NOMATCH) ||
			(!boost::filesystem::is_regular_file(di->path())))
		{
			continue;
		}
		const GdalInfo* gdalInfo = addGdalInfo(baseName, getGdalDatasetHolder(baseName));
		if (op(*gdalInfo)) {
			return true;
		}
	}
	_allSeen = true;
	return false;
}

const void *CachedGdalBase::getTileVector(int band, double latDeg, double lonDeg,
	int *pixelIndex)
{
	checkBandIndex(band);
	if (!findTile(band, latDeg, lonDeg)) {
		return nullptr;
	}
	const TileInfo &tileInfo(*_tileCache.recentValue());
	int tileLatIdx, tileLonIdx;
	tileInfo.transformation.computePixel(latDeg, lonDeg, &tileLatIdx, &tileLonIdx);
	*pixelIndex = tileInfo.transformation.lonSize * tileLatIdx + tileLonIdx;
	return tileInfo.tileVector.get();
}

bool CachedGdalBase::getPixelDirect(int band, double latDeg, double lonDeg, void *pixelBuf)
{
	checkBandIndex(band);
	// First need to find pixel whereabouts in file
	const GdalInfo *gdalInfo;
	int fileLatIdx, fileLonIdx;
	if (!getGdalPixel(latDeg, lonDeg, &gdalInfo, &fileLatIdx, &fileLonIdx)) {
		return false;	// GDAL file not found
	}
	// Bringing GDAL data set and reading from it
	const GdalDatasetHolder *datasetHolder = getGdalDatasetHolder(gdalInfo->baseName);
	CPLErr readError = datasetHolder->gdalDataset->GetRasterBand(band)->RasterIO(
		GF_Read, fileLonIdx, fileLatIdx, 1, 1, pixelBuf, 1, 1, _pixelType, 0, 0);
	if (readError != CPLErr::CE_None) {
		std::ostringstream errStr;
		errStr << "ERROR: CachedGdalBase::getPixelDirect(): Reading GDAL pixel from '" <<
			gdalInfo->baseName << "' (band: " << band <<
			", xOffset: " << fileLonIdx << ", yOffset: " << fileLatIdx << ") failed: " <<
			CPLGetLastErrorMsg();
		throw std::runtime_error(errStr.str());
	}
	return true;
}

bool CachedGdalBase::findTile(int band, double latDeg, double lonDeg)
{
	// Maybe recent tile would suffice?
	// Double boundary check is necessary to cover the case of noninteger margin
	// (not reflected in tile boundary, but reflected in GDAL boundary)
	if (_tileCache.recentValue() &&
		(_tileCache.recentValue()->boundRect.contains(latDeg, lonDeg)) &&
		(_tileCache.recentValue()->gdalInfo->boundRect.contains(latDeg, lonDeg)) &&
		(_tileCache.recentKey()->band == band))
	{
		return true;
	}
	// Will look up in cache. First need to find pixel whereabouts in file
	const GdalInfo *gdalInfo;
	int fileLatIdx, fileLonIdx;
	if (!getGdalPixel(latDeg, lonDeg, &gdalInfo, &fileLatIdx, &fileLonIdx)) {
		return false;
	}
	// Key for tile cache
	int intMargin = int(std::floor(gdalInfo->transformation.margin));
	TileKey tileKey(band,
		std::max(fileLatIdx - (fileLatIdx % _maxTileSize), intMargin),
		std::max(fileLonIdx - (fileLonIdx % _maxTileSize), intMargin),
		gdalInfo->baseName);
	// Trying to bring tile from cache
	if (_tileCache.get(tileKey)) {
		return true;
	}
	// Tile not in cache - will add it. First building TileInfo object
	int latTileSize =
		std::min(_maxTileSize,
		gdalInfo->transformation.latSize - tileKey.latOffset - intMargin);
	int lonTileSize =
		std::min(_maxTileSize,
		gdalInfo->transformation.lonSize - tileKey.lonOffset - intMargin);
	TileInfo tileInfo(this,
		GdalTransform(gdalInfo->transformation, tileKey.latOffset, tileKey.lonOffset,
		latTileSize, lonTileSize),
		gdalInfo);

	// Now reading pixel data into buffer of tile object
	const GdalDatasetHolder *datasetHolder = getGdalDatasetHolder(gdalInfo->baseName);
	CPLErr readError = datasetHolder->gdalDataset->GetRasterBand(tileKey.band)->RasterIO(
		GF_Read, tileKey.lonOffset, tileKey.latOffset, lonTileSize, latTileSize,
		getTileBuffer(tileInfo.tileVector.get()), lonTileSize, latTileSize, _pixelType, 0, 0);
	if (readError != CPLErr::CE_None) {
		std::ostringstream errStr;
		errStr << "ERROR: CachedGdalBase::findTile(): Reading GDAL data from '" <<
			tileKey.baseName << "' (band: " << tileKey.band <<
			", xOffset: " << tileKey.lonOffset << ", yOffset: " << tileKey.latOffset <<
			", xSize: " << lonTileSize << ", ySize: " << latTileSize << ") failed: " <<
			CPLGetLastErrorMsg();
		throw std::runtime_error(errStr.str());
	}
	LOGGER_DEBUG(logger) << "[" << latTileSize << " X " << lonTileSize <<
		"] tile retrieved from (" << tileKey.latOffset << ", " <<
		tileKey.lonOffset << ") of band " << tileKey.band << " of '" <<
		gdalInfo->baseName << "'";
	// Finally adding tile to cache
	_tileCache.add(tileKey, tileInfo);
	return true;
}

bool CachedGdalBase::getGdalPixel(double latDeg, double lonDeg,
	const GdalInfo **gdalInfo, int *fileLatIdx, int *fileLonIdx)
{
	// First look for GdalInfo containing given point.
	// For monolithic data - it is recent (and the only) GdalInfo object
	*gdalInfo = _recentGdalInfo;
	if (!isMonolithic()) {
		// If not recent - will look further
		if (!_recentGdalInfo->boundRect.contains(latDeg, lonDeg)) {
			// Name of file
			std::string baseName = _nameMapper->nameFor(latDeg, lonDeg);
			// No such file?
			if (baseName.empty()) {
				return false;
			}
			bool known = getGdalInfo(baseName, gdalInfo);
			// Is file known to not exist?
			if (known && (!*gdalInfo)) {
				return false;
			}
			// If file is unknown - let's try to bring it
			if (!known) {
				boost::filesystem::path filePath(_fileOrDir);
				filePath /= baseName;
				boost::system::error_code systemErr;
				// Does this file exist?
				if (!boost::filesystem::is_regular_file(filePath, systemErr)) {
					addGdalInfo(baseName, nullptr);
					return false;
				}
				*gdalInfo = addGdalInfo(baseName, getGdalDatasetHolder(baseName));
			}
		}
	}
	// GdalInfo found. Does it contain given point?
	if (!(*gdalInfo)->boundRect.contains(latDeg, lonDeg)) {
		return false;
	}
	// GDAL file found. Computing indices in it
	(*gdalInfo)->transformation.computePixel(latDeg, lonDeg, fileLatIdx, fileLonIdx);
	return true;
}

const CachedGdalBase::GdalDatasetHolder *CachedGdalBase::getGdalDatasetHolder(
	const std::string &baseName)
{
	if ((_gdalDsCache.recentKey() && (baseName == *_gdalDsCache.recentKey())) ||
		_gdalDsCache.get(baseName))
	{
		return _gdalDsCache.recentValue()->get();
	}
	boost::filesystem::path filePath(_fileOrDir);
	if (!isMonolithic()) {
		filePath /= baseName;
	}
	auto ret =
		_gdalDsCache.add(baseName,
		std::shared_ptr<GdalDatasetHolder>(new GdalDatasetHolder(filePath.native())))->get();
	return ret;
}

const CachedGdalBase::GdalInfo *CachedGdalBase::addGdalInfo(const std::string &baseName,
	const GdalDatasetHolder *gdalDatasetHolder)
{
	if (gdalDatasetHolder) {
		auto p =
			_gdalInfos.emplace(
			baseName,
			std::move(std::unique_ptr<GdalInfo>(new GdalInfo(gdalDatasetHolder,
			_numBands, _transformationModifier))));
		_recentGdalInfo = p.first->second.get();
		LOGGER_DEBUG(logger) << "GDAL file '" << gdalDatasetHolder->fullFileName <<
			"' covers area from [" <<
			formatPosition(_recentGdalInfo->boundRect.latDegMin,
			_recentGdalInfo->boundRect.lonDegMin) << "] (Lower Left) to [" <<
			formatPosition(_recentGdalInfo->boundRect.latDegMax,
			_recentGdalInfo->boundRect.lonDegMax) << "] (Upper Right). "
			"Image resolution " << formatDms(_recentGdalInfo->transformation.latPixPerDeg) <<
			" by " << formatDms(_recentGdalInfo->transformation.lonPixPerDeg) <<
			" pixels per degree. Image size is " <<
			_recentGdalInfo->transformation.latSize << " by " <<
			_recentGdalInfo->transformation.lonSize << " pixels";
		return _recentGdalInfo;
	}
	_gdalInfos.emplace(baseName, nullptr);
	return nullptr;
}

bool CachedGdalBase::getGdalInfo(const std::string &filename,
	const CachedGdalBase::GdalInfo **gdalInfo)
{
	auto iter = _gdalInfos.find(filename);
	if (iter == _gdalInfos.end()) {
		*gdalInfo = nullptr;
		return false;
	}
	if (!iter->second.get()) {
		*gdalInfo = nullptr;
		return true;
	}
	_recentGdalInfo = iter->second.get();
	*gdalInfo = _recentGdalInfo;
	return true;
}

double CachedGdalBase::gdalNoData(int band) const
{
	return _recentGdalInfo->noDataValues[band - 1];
}

void CachedGdalBase::checkBandIndex(int band) const
{
	if ((unsigned)(band - 1) < (unsigned)_numBands) {
		return;
	}
	std::ostringstream errStr;
	errStr << "ERROR: CachedGdalBase::checkBandIndex(): Invalid band index " <<
		band << ". Should be in [1.." << _numBands << "] range";
	throw std::runtime_error(errStr.str());
}

bool CachedGdalBase::covers(double latDeg, double lonDeg)
{
	return forEachGdalInfo(
		[latDeg, lonDeg](const GdalInfo &gdalInfo)
		{return gdalInfo.boundRect.contains(latDeg, lonDeg);});
}

GdalTransform::BoundRect CachedGdalBase::boundRect()
{
	GdalTransform::BoundRect ret(_recentGdalInfo->boundRect);
	if (!isMonolithic()) {
		forEachGdalInfo(
			[&ret](const GdalInfo &gdalInfo)
			{ret.combine(gdalInfo.boundRect); return false;});
	}
	return ret;
}

boost::optional<CachedGdalBase::PixelInfo> CachedGdalBase::getPixelInfo(
	double latDeg, double lonDeg)
{
	const GdalInfo *gdalInfo;
	int fileLatIdx, fileLonIdx;
	if (!getGdalPixel(latDeg, lonDeg, &gdalInfo, &fileLatIdx, &fileLonIdx)) {
		return boost::none;
	}
	return PixelInfo(gdalInfo->baseName, fileLatIdx, fileLonIdx);
}

std::string CachedGdalBase::formatDms(double deg, bool forceDegrees)
{
	std::string ret;
	if (deg < 0) {
		ret += "-";
		deg = -deg;
	}
	bool started = false;
	int d = (int)floor(deg);
	if (d || forceDegrees) {
		ret += std::to_string(d);
		ret += "d";
		started = true;
	}
	char buf[50];
	deg = (deg - d) * 60.;
	int m = (int)floor(deg);
	if (m || started) {
		snprintf(buf, sizeof(buf), started ? "%02d'" : "%d'", m);
		ret += buf;
		started = true;
	}
	snprintf(buf, sizeof(buf), started ? "%05.2f\"" : "%0.2f\"", (deg - m) * 60.);
	ret += buf;
	return ret;
}

std::string CachedGdalBase::formatPosition(double latDeg, double lonDeg)
{
	std::string ret;
	ret += formatDms(fabs(latDeg), true);
	ret += (latDeg >= 0) ? "N, " : "S, ";
	ret += formatDms(fabs(lonDeg), true);
	ret += (lonDeg >= 0) ? "E" : "W";
	return ret;
}

GDALDataType CachedGdalBase::gdalDataType(uint8_t) {return GDT_Byte;}
GDALDataType CachedGdalBase::gdalDataType(uint16_t) {return GDT_UInt16;}
GDALDataType CachedGdalBase::gdalDataType(int16_t) {return GDT_Int16;}
GDALDataType CachedGdalBase::gdalDataType(uint32_t) {return GDT_UInt32;}
GDALDataType CachedGdalBase::gdalDataType(int32_t) {return GDT_Int32;}
GDALDataType CachedGdalBase::gdalDataType(float) {return GDT_Float32;}
GDALDataType CachedGdalBase::gdalDataType(double) {return GDT_Float64;}
