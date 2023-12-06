/*
 * Copyright (C) 2022 Broadcom. All rights reserved.
 * The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
 * that owns the software below.
 * This work is licensed under the OpenAFC Project License, a copy of which is
 * included with this software program.
 */

/** @file
 * GDAL-based geospatial data accessor.
 *
 * Geospatial data (terrain heights, terrain type, etc.) come in image files
 * (usually TIFF or TIFF-like) processed through GDAL library - hereinafter
 * named GDAL files, containing GDAL data. Data values are pixel 'color'
 * components in these files. Pixel data type may be 8/16/32 bit signed/unsigned
 * integer or 32/64 bit floating point. Data may be contained in a monolithic
 * file or be split to tile files each covering some rectangular piece of terrain
 * (usually 1 degree by 1 degree). Tile files are contained in a single directory
 * and have some unified name structure. Each file may contain one or more piece
 * of data per pixel (e.g. terrain height and building height) - such data layers
 * stored as color components, named bands (there may be up to 4 bands in a file).
 *
 * GDAL data may contain data tied to any coordinate system (grid or
 * latitude/longitude), but this module assumes that coordinate system is
 * geodetic north-up (longitude east-positive, latitude north-positive),
 * coordinates expressed in degrees, datum is WGS84. Heights assumed to be
 * ellipsoidal (not orthometric). It is user responsibility to convert data
 * according to these assumptions.
 *
 * Besides pixel data, GDAL files contain transformation information that
 * allows to find pixel coordinates for given latitude and longitude. This
 * information might be slightly imprecise and API of this module provide a means
 * for rectifying it.
 *
 * This module provides access to monolithic and tiled GDAL data sources. GDAL
 * access performance is improved by caching tiles of recently accessed geodetic
 * data in LRU cache.
 *
 * Usage notes:
 *
 * INITIALIZATION
 *
 * Creation of GDAL data source object (template parameter is pixel data type,
 * second constructor parameter is data set name, not used internally but useful
 * for logging, etc.) for various GDAL sources:
 * 
 * - NLCD data (land usage information). May come in several forms:
 *   - Single file:
 *	nlcd = CachedGdal<uint8_t> nlcd("nlcd/federated_nlcd.tif", "nlcd");
 *   - Directory with several files (not many, otherwise startup will be slow):
 *  nlcd = CachedGdal<uint8_t>("nlcd/nlcd_production", "nlcd",
 *  	GdalNameMapperDirect::make_unique("*.tif", "nlcd/nlcd_production"));
 *   - Direcory with tiled NLCD files (pixels numbered from top left corner):
 *  nlcd = CachedGdal<uint8_t>("tiled_nlcd/nlcd_production", "nlcd",
 *  	GdalNameMapperPattern::make_unique(
 * 			"nlcd_production_{latHem:ns}{latDegCeil:02}{lonHem:ew}{lonDegFloor:03}.tif",
 *  		"tiled_nlcd/nlcd_production"));
 *
 * - Single LiDAR file data (monolithic 2-band file with 32-bit float data):
 *	lidar = CachedGdal<float>("San_Francisco_20080708-11/san_francisco_ca_0_01.tif,
 *		"LiDAR", nullptr, 2);
 *
 * - 3DEP tiled data (tile files with 32-bit float data, pixels numbered from
 *   top left corner):
 *	dep = CachedGdal<float>("3dep\1_arcsec", "3dep",
 *		GdalNameMapperPattern::make_unique(
 *		"USGS_1_{latHem:ns}{latDegCeil:}{lonHem:ew}{lonDegFloor:}*.tif")));
 *	dep.setTransformationModifier(
 *		[](CachedGdalBase::Transformation *t)
 *		{t->roundPpdToMultipleOf(1.); t->setMarginsOutsideDeg(1.);})
 *
 * - SRTM data (tile files with 16-bit integer data, margins are half-pixel
 *   wide, pixels numbered from bottom left corner). In previous implementation
 *   coordinate system was shifted by half a pixes down and right. This
 *   initialization doesn't do this shift:
 *	srtm = CachedGdal<int16_t>("srtm3arcsecondv003", "srtm",
 *		GdalNameMapperPattern::make_unique(
 *		"{latHem:NS}{latDegFloor:02}{lonHem:EW}{lonDegFloor:03}.hgt"));
 *	srtm.setTransformationModifier(
 *		[](CachedGdalBase::Transformation *t)
 *		{t->roundPpdToMultipleOf(0.5); t->setMarginsOutsideDeg(1.);});
 *
 * - Globe data (tile files with 16-bit integer data):
 *	globe = CachedGdal<int16_t>("globe", "globe",
 *		GdalNameMapperDirect::make_unique("globe", "*.bil"));
 *
 * DATA RETRIEVAL
 *
 * - Indirect retrieval of data for 38N, 122E from 2-nd band of LiDAR data:
 *	float h;
 *	if (lidar.getValue(38, -122, &h, 2)) {
 *		// Found, value in 'h'
 *  } else {
 *		// Not found
 *	}
 *
 * - Direct retrieval of SRTM value:
 *	constexpr int16_t SRTM_NO_DATA = -1000;
 *	srtm.setNoData(SRTM_NO_DATA); // Once, somewhere after initialization
 *	....
 *	int16_t h = srtm.valueAt(38, -122);
 *	if (h == SRTM_NO_DATA) {
 *		// Value not found
 *	} else {
 *		// Value found
 *	}
 *
 * General implementation notes:
 * Core logic is implemented in abstract base class CachedGdalBase.
 * Instantiable are derived template classes CachedGdal<PixelDataType>,
 * parameterized by C type of pixel data stored in GDAL files.
 *
 * CachedGdalBase contains LRU cache (limited capacity map) of tiles (square/
 * rectangular extracts of pixel data from single band). Cache capacity and
 * maximum tile size are optional parameters of CachedGdal<PixelDataType>
 * constructor.
 *
 * Note here the ambiguity of the word 'tile' in GDAL context. Tile in the
 * context of this module is a unit of caching (in-memory rectangular piece of
 * geospatial data), yet in the context of GDAL tile might be a rectangular piece
 * of geospatial data, stored in one file of multifile (tiled) data source (such
 * as 3DEP).
 *
 * The essential part (looking for tile, containing data for given
 * latitude/longitude) is in CachedGdalBase::findTile().
 *
 * Brief overview of classes:
 *	- CachedGdal<PixelDataType>. GDAL data manager (derived from CachedGdalBase).
 *		Objects of this class are used by the rest of application to access GDAL
 *		data. This class is parameterized by pixel data type (char, short, int,
 *		float, double).
 *	- CachedGdalBase. Abstract base class for GDAL data manager. Implements the
 *		core logic.
 *  - CachedGdalBase::GdalDatasetHolder. RAII wrapper around GDALDataset objects.
 *	- CachedGdalBase::PixelInfo. Information about pixel whereabouts in GDAL file
 *		(file name, row, column).
 *	- CachedGdalBase::TileKey. Key in the cache of retrieved tiles. Describes
 *		tile whereabouts
 *	- CachedGdalBase::TileInfo. Information about tile in the tile cache, also
 *		holds pixel data of this tile
 *	- CachedGdalBase::GdalInfo. Information about single GDAL file
 *	- GdalTransform. Data pertinent to latitude/longitude to pixel row/column
 *		conversion
 *	- GdalTransform::BoundRect. Rectangle around data stored in tile or file.
 *		For checking if latitude/longitude is covered
 *	- GdalNameMapperBase. Abstract base class for name for name mappers that
 *		provides information about file names in tiled GDAL data
 *	- GdalNameMapperPattern. Concrete name mapper class built around file name
 *		pattern
 *	- GdalNameMapperDirect. Concrete name mapper that probes all tile files and
 *		retrieves mapping information from them. For use in those unfortunate
 *		cases, when name-based mapping is not obvious and number of tile files
 *		is relatively small (e.g. Globe data)
 *	- LruValueCache. LRU cache - copyless improvement of boost::lru_cache
 */

#ifndef CACHED_GDAL_H
#define CACHED_GDAL_H

#include <functional>
#include "GdalNameMapper.h"
#include "GdalTransform.h"
#include <gdal_priv.h>
#include "LruValueCache.h"
#include <map>
#include <memory>
#include <boost/core/noncopyable.hpp>
#include <boost/optional.hpp>
#include <string>
#include <vector>

/** @file
 * Unified GDAL geospatial data access module */

/** Abstract base class that handles everything but pixel data */
class CachedGdalBase : private boost::noncopyable {
public:
	//////////////////////////////////////////////////
	// CachedGdalBase. Public class constants
	//////////////////////////////////////////////////

	/** Default maximum tile side (number of pixels in one dimension) size */
	static const int DEFAULT_MAX_TILE_SIZE = 1000;

	/** Default maximum size of LRU cache of tiles */
	static const int DEFAULT_CACHE_SIZE = 50;

	/** Maximum number of simultaneously opened GDAL files */
	static const int GDAL_CACHE_SIZE = 9;

	//////////////////////////////////////////////////
	// CachedGdalBase. Public class types
	//////////////////////////////////////////////////

	/** Holds GDALDataset pointer and full filename.
	 * RAII wrapper around GDALDataset*
	 */
	struct GdalDatasetHolder {
		/** Constructor - opens dataset.
		 * @param fullFileName Full file name of GDAL file
		 */
		GdalDatasetHolder(const std::string& fullFileName);

		/** Destructor - closes dataset */
		~GdalDatasetHolder();

		/** Dataset pointer */
		GDALDataset* gdalDataset;

		/** Full file name of GDAL file */
		std::string fullFileName;
	};

	/** Information of whereabouts of data for certain pixel */
	struct PixelInfo {
		/** Constructor
		 * @param baseName Base name of GDAL file containing pixel
		 * @param row 0-based row number in GDAL file
		 * @param column 0-based column number in GDAL file
		 */
		PixelInfo(const std::string &baseName, int row, int column);

		/** Default constructor */
		PixelInfo();

		/** Base name of GDAL file containing pixel */
		std::string baseName;
		/** 0-based row number in GDAL file */
		int row;
		/** 0-based column number in GDAL file */
		int column;
	};

	//////////////////////////////////////////////////
	// CachedGdalBase. Public member functions
	//////////////////////////////////////////////////

	/** Virtual destructor */
	virtual ~CachedGdalBase() = default;

	/** Data set name */
	const std::string &dsName() const;

	/** Sets callback that modifies (rectifies) transformation data retrieved
	 * from GDAL file. */
	void setTransformationModifier(std::function<void(GdalTransform *)> modifier);

	/** True for monolithic data source, false for tiled directory */
	bool isMonolithic() const;

	/** Check if given point is covered by GDAL data.
	 * Current version only works for monolithic data
	 */
	bool covers(double latDeg, double lonDeg);

	/** Retrieves geospatial data boundaries.
	 * Current version only works for monolithic data
	 * @param[out] lonDegMax Optional maximum longitude in east-positive degrees
	 */
	GdalTransform::BoundRect boundRect();

	/** Returns whereabouts of data for given latitude/longitude in GDAL file.
	 * This function is for comparison with other implementations
	 * @param latDeg Latitude in north-positive degrees
	 * @param lonDeg Longitude in east-positive degrees
	 * @return Optional information about pixel whereabouts
	 */
	boost::optional<PixelInfo> getPixelInfo(double latDeg, double lonDeg);

	//////////////////////////////////////////////////
	// CachedGdalBase. Public static methods
	//////////////////////////////////////////////////

	/** Format degree value into degree/minute/second.
	 * @param deg Degree value
	 * @param forceDegrees True to present leading degrees/minutes even if they
	 *	are zero
	 * @return String representation
	 */
	static std::string formatDms(double deg, bool forceDegrees = false);

	/** String representation of given position.
	 * @param latDeg North-positive latitude in degrees
	 * @param lonDeg East-positive longitude in degrees
	 * @return String representation of given position
	 */
	static std::string formatPosition(double latDeg, double lonDeg);

protected:

	/** Constructor.
	 * @param fileOrDir Name of file (for monolithic file data GDAL source) or
	 *	name of directory (for multifile directory data source)
	 * @param dsName Data set name (not used internally - for logging purposes)
	 * @param nameMapper Null for monolithic (single-file) data, address of
	 *	GdalNameMapper object for multifile (tiled) data
	 * @param numBands Number of bands that will be used (i.e. maximum 1-based
	 *	band index)
	 * @param maxTileSize Maximum size for tile in one dimension
	 * @param cacheSize Maximum number of tiles in tile cache
	 * @param pixelType Value describing pixel data type in RasterIO operation
	 */
	CachedGdalBase(std::string fileOrDir, const std::string &dsName,
		std::unique_ptr<GdalNameMapperBase> nameMapper, int numBands,
		int maxTileSize, int cacheSize, GDALDataType pixelType);

	/** Value for unavailable data for given band, as obtained from GDAL */
	double gdalNoData(int band) const;

	/** Post-construction initialization.
	 * Initialization functionality that requires virtual functions of derived
	 * classes, unavailable at this class' construction time
	 */
	void initialize();

	/** Pre-destruction cleanup (stuff that requires virtual functions,
	 * unavailable in destructor)
	 */
	void cleanup();

	/** Looks up tile, containing pixel for given coordinates.
	 * @param[in] band 1-based index of band in GDAL file
	 * @param[in] latDeg North-positive latitude in degrees
	 * @param[in] lonDeg East-positive longitude in degrees
	 * @param[out] pixelIndex Index of pixel data inside tile vector
	 * @return std::vector, containing tile pixel data, nullptr if lookup failed
	 */
	const void *getTileVector(int band, double latDeg, double lonDeg, int *pixelIndex);

	/** Read pixel data directly, bypassing caching mechanism
	 * @param[in] band 1-based index of band in GDAL file
	 * @param[in] latDeg North-positive latitude in degrees
	 * @param[in] lonDeg East-positive longitude in degrees
	 * @param[out] pixelBuf Buffer to read pixel into
	 * @return True on success, false on fail
	 */ 
	bool getPixelDirect(int band, double latDeg, double lonDeg, void *pixelBuf);

	/** Throws if given band index is invalid */
	void checkBandIndex(int band) const;


	//////////////////////////////////////////////////
	// CachedGdalBase. Pixel-type specific tile manipulation pure virtual functions
	//////////////////////////////////////////////////

	/** Creates on heap a std::vector buffer for tile pixel data.
	 * @param latSize Pixel count in latitude direction
	 * @param lonSize Pixel count in longitude direction
	 * @return Address of created std::vector
	 */
	virtual void *createTileVector(int latSize, int lonSize) const = 0;

	/** Deletes tile vector.
	 * @param tileVector std::vector to delete
	 */
	virtual void deleteTileVector(void *tileVector) const = 0;

	/** Returns address of tile's data buffer
	 * @param tileVector std::vector, containing tile pixel data
	 * @return Vector's buffer, containing pixel data
	 */
	virtual void *getTileBuffer(void *tileVector) const = 0;


	//////////////////////////////////////////////////
	// CachedGdalBase. Protected static methods
	//////////////////////////////////////////////////

	// Functions that map pixel types to respective GDAL data type codes
	static GDALDataType gdalDataType(uint8_t);
	static GDALDataType gdalDataType(uint16_t);
	static GDALDataType gdalDataType(int16_t);
	static GDALDataType gdalDataType(uint32_t);
	static GDALDataType gdalDataType(int32_t);
	static GDALDataType gdalDataType(float);
	static GDALDataType gdalDataType(double);

private:

	//////////////////////////////////////////////////
	// CachedGdalBase. Private class types
	//////////////////////////////////////////////////

	//////////////////////////////////////////////////
	// CachedGdalBase::GdalInfo
	//////////////////////////////////////////////////

	/** Information pertinent to a single GDAL file */
	struct GdalInfo {

		//////////////////////////////////////////////////
		// CachedGdalBase::GdalInfo. Public instance methods
		//////////////////////////////////////////////////

		/** Constructor.
		 * @param gdalDataset GdalDatasetHolder for file being added
		 * @param minBands Minimum required number of bands
		 * @param transformationModifier Optional transformation modifier
		 */
		GdalInfo(const GdalDatasetHolder *gdalDataset, int minBands,
			const boost::optional<std::function<void(GdalTransform *)>>
			&transformationModifier);

		//////////////////////////////////////////////////
		// CachedGdalBase::GdalInfo. Public instance data
		//////////////////////////////////////////////////

		/** File name without directory */
		std::string baseName;

		/** Transformation of coordinates to pixel indices */
		GdalTransform transformation;

		/** Boundary rectangle with margins (if any) applied */
		GdalTransform::BoundRect boundRect;

		/** Number of bands */
		int numBands;

		/** Per-band no-data values [0] contains value for band 1, etc. */
		std::vector<double> noDataValues;
	};

	//////////////////////////////////////////////////
	// CachedGdalBase::TileKey
	//////////////////////////////////////////////////

	/** Tile identifier in cache */
	struct TileKey {

		//////////////////////////////////////////////////
		// CachedGdalBase::TileKey. Public instance methods
		//////////////////////////////////////////////////

		/** Constructor.
		* @param band 1-based band index
		* @param latOffset Tile offset in latitude direction
		* @param lonOffset Tile offset in longitude direction
		* @param baseName Tile file base name
		*/
		TileKey(int band, int latOffset, int lonOffset, const std::string &baseName);

		/** Default constructor */
		TileKey();

		/** Ordering comparison */
		bool operator <(const TileKey &other) const;

		//////////////////////////////////////////////////
		// CachedGdalBase::TileKey. Public instance data
		//////////////////////////////////////////////////

		/** 1-based band index */
		int band;

		/** Tile offset in latitude direction */
		int latOffset;

		/** Tile offset in longitude direction */
		int lonOffset;

		/** Tile file base name */
		std::string baseName;
	};

	//////////////////////////////////////////////////
	// CachedGdalBase::TileInfo
	//////////////////////////////////////////////////

	/** Tile data in cache */
	struct TileInfo {

		//////////////////////////////////////////////////
		// CachedGdalBase::TileInfo. Public instance methods
		//////////////////////////////////////////////////

		/** Constructor.
		* @param cachedGdal Parent container
		* @param transformation Pixel indices computation transformation
		* @param gdalInfo GdalInfo containing this tile
		*/
		TileInfo(CachedGdalBase *cachedGdal, const GdalTransform &transformation,
			const GdalInfo *gdalInfo);

		/** Default constructor to appease boost::lru_cache */
		TileInfo();


		//////////////////////////////////////////////////
		// CachedGdalBase::TileInfo. Public instance data
		//////////////////////////////////////////////////

		/** Parent container */
		const CachedGdalBase *cachedGdal;

		/** Transformation of coordinates to pixel indices */
		GdalTransform transformation;

		/** Tile boundary rectangle.
		* Always contain whole number of pixels, noninteger boundaries checked
		* through gdalInfo->boundRect
		*/
		GdalTransform::BoundRect boundRect;

		/* GdalInfo containing this tile */
		const GdalInfo *gdalInfo;

		/** std::vector that contains tile pixel data */
		std::shared_ptr<void> tileVector;
	};


	//////////////////////////////////////////////////
	// CachedGdalBase. Private instance methods
	//////////////////////////////////////////////////

	/** Tries to find tile for given coordinates and bands
	 * @param[in] band 1-based band index
	 * @param[in] latDeg Latitude of point to look tile of in north-positive
	 *	degrees
	 * @param[in] lonDeg Longitude of point to look tile of in east-positive
	 *	degrees
	 * @return On success makes desired tile the recent in tile cache and returns
	 *	true, otherwise returns false
	 */
	bool findTile(int band, double latDeg, double lonDeg);

	/** Provides GDAL whereabouts of data for point with given coordinates.
	 * @param latDeg[in] North-positive latitude in degrees
	 * @param lonDeg[in] East-positive longitude in degrees
	 * @param gdalInfo[out] GdalInfo of file, containing point (if found)
	 * @param fileLatIdx[out] Latitude index (row) in GDAL file (if found)
	 * @param fileLonIdx[out] Longitude index (column) in file (if found)
	 * @return True if pixel for given point found, false otherwise
	 */
	bool getGdalPixel(double latDeg, double lonDeg, const GdalInfo **gdalInfo,
		int *fileLatIdx, int *fileLonIdx);

	/** Brings in GDAL dataset holder that corresponds to given file name (file
	 * must exist).
	 * @param baseName Base name of file to bring in
	 * @return Pointer to holder of GDALDataset of given file
	 */
	const GdalDatasetHolder *getGdalDatasetHolder(const std::string& filename);

	/** Adds GdalInfo information for given file to collection of known GDAL files
	 * @param baseName GDAL file base name
	 * @param gdalDataset Holder of GDALDataset for existing file, nullptr for
	 *	nonexistent file
	 * @return Address of created GdalInfo object
	 */
	const GdalInfo* addGdalInfo(const std::string& baseName,
		const GdalDatasetHolder* gdalDataset);

	/* Lookup of GdalInfo for given file name.
	 * @param[in] baseName File base name
	 * @param[out] gdalInfo Address of found (or not found) GdalInfo object
	 * @return True on lookup success (in case of lookup for nonexistent files,
	 * true is still returned, but *gdalInfo filled with nullptr)
	 */
	bool getGdalInfo(const std::string& baseName, const GdalInfo** gdalInfo);

	/** Calls given function for GdalInfo objects, corresponding to some or all
	 * GDAL files
	 * @param op Function to call for each GdalInfo object. Function return true
	 *	to stop iteration (e.g. if something desirable was found), false to
	 *	continue
	 * @return True if last call of op() returned true
	 */
	bool forEachGdalInfo(const std::function<bool(const GdalInfo& gdalInfo)>& op);

	/** Does proper cleanup and reinitialization after GDAL parameters modification
	 * For use after mapping parameter change
	 */
	void rereadGdal();


	//////////////////////////////////////////////////
	// CachedGdalBase. Private instance data
	//////////////////////////////////////////////////

	/** Name of file or directory of tiled files */
	const std::string _fileOrDir;

	/** Data set name */
	std::string _dsName;

	/** For tiled  (multifile) data source - provides base name of tile file
	 * for given latitude/longitude. Null for monolithic data source
	 */
	std::unique_ptr<GdalNameMapperBase> _nameMapper;

	/** Optional transformation modifier (rectifier) callback */
	boost::optional<std::function<void(GdalTransform *)>> _transformationModifier;

	/** Number of bands to be used (maximum 1-based band index) */
	const int _numBands;

	/** Pixel data type for RasterIO() call */
	const GDALDataType _pixelType;

	/** Maximum size for tile in one dimension */
	const int _maxTileSize;

	/** LRU tile cache */
	LruValueCache<TileKey, TileInfo> _tileCache;

	/** GDAL dataset holders indexed by base file names */
	LruValueCache<std::string, std::shared_ptr<GdalDatasetHolder>> _gdalDsCache;

	/** Maps base filenames to GdalInfo objects (null pointers for nonexistent
	 * files)
	 */
	std::map<std::string, std::unique_ptr<GdalInfo>> _gdalInfos;

	/** Recently used GdalInfo object.
	 * After initial initialization is always nonnull. May only be changed by
	 * addGdalInfo() and getGdalInfo()
	 */
	const GdalInfo* _recentGdalInfo;

	/** True if information about all GDAL files retrieved to _gdalInfos */
	bool _allSeen;
};

/** Concrete GDAL cache class, parameterized by pixel data type */
template <class PixelData>
class CachedGdal : public CachedGdalBase {
public:
	/** Constructor.
	 * @param fileOrDir Name of file (for monolithic file data GDAL source) or
	 *	of directory (for multifile data source)
	 * @param dsName Data set name (not used internally - for logging purposes)
	 * @param nameMapper Null for monolithic (single-file) data, address of
	 *	GdalNameMapperBase-derived  object for multifile (tiled) data
	 * @param numBands Number of bands that will be used (maximum value for
	 *	1-based band index)
	 * @param maxTileSize Maximum size for tile in one dimension
	 * @param cacheSize Maximum number of tiles in LRU cache
	 */
	CachedGdal(const std::string &file_or_dir, const std::string &dsName,
		std::unique_ptr<GdalNameMapperBase> nameMapper = nullptr, int numBands = 1,
		int maxTileSize = CachedGdalBase::DEFAULT_MAX_TILE_SIZE,
		int cacheSize = CachedGdalBase::DEFAULT_CACHE_SIZE)
		: CachedGdalBase(file_or_dir, dsName, std::move(nameMapper), numBands,
		maxTileSize, cacheSize, CachedGdalBase::gdalDataType((PixelData)0))
	{
		initialize();
	}

	/** Virtual destructor */
	virtual ~CachedGdal()
	{
		cleanup();
	}

	/** Retrieves geospatial data value by output parameter
	 * @param[in] latDeg North-positive latitude in degrees
	 * @param[in] lonDeg East-positive longitude in degrees
	 * @param[out] value Geospatial value
	 * @param[in] band 1-based band index
	 * @param[in] direct True to read pixel directly, bypassing caching
	 *	mechanism (may speed up accessing scattered data)
	 * @return True on success, false if coordinates are outside of file(s)
	 */
	bool getValueAt(double latDeg, double lonDeg, PixelData *value, int band = 1,
		bool direct = false)
	{
		PixelData v;
		bool ret; // True if retrieval successful
		if (direct) {
			// Directly reading pixel
			ret = getPixelDirect(band, latDeg, lonDeg, &v);
		} else {
			// First - finding tile
			int pixelIndex;
			auto tileVector = reinterpret_cast<const std::vector<PixelData> *>(
				getTileVector(band, latDeg, lonDeg, &pixelIndex));
			ret = tileVector != nullptr;
			if (ret) {
				// if tile found - retrieving pixel from it
				v = tileVector->at(pixelIndex);
			}
		}
		if (ret && (v == static_cast<PixelData>(gdalNoData(band)))) {
			// If 'no-data' pixel was retrieved - count as faiilure
			ret = false;
		}
		if (value) {
			// Caller needs pixel value
			if (!ret) {
				// Value for 'no-data' pixel - overridden or from GHDAL file
				auto ndi = _noData.find(band);
				v = (ndi != _noData.end()) ? ndi->second : static_cast<PixelData>(gdalNoData(band));
			}
			*value = v;
		}
		return ret;
	}

	/** Retrieves geospatial data value by return result
	 * @param[in] latDeg North-positive latitude in degrees
	 * @param[in] lonDeg East-positive longitude in degrees
	 * @param[in] band 1-based band index
	 * @param[in] direct True to read pixel directly, bypassing caching
	 *	mechanism (may speed up accessing scattered data)
	 * @return Resulted geospatial value
	 */
	PixelData valueAt(double latDeg, double lonDeg, int band = 1,
		bool direct = false)
	{
		PixelData ret;
		getValueAt(latDeg, lonDeg, &ret, band, direct);
		return ret;
	}

	/** Sets value used when no data is available for given band */
	void setNoData(PixelData value, int band = 1)
	{
		checkBandIndex(band);
		_noData[band] = value;
	}

	/** Returns value used when no data is available for given band */
	PixelData noData(int band = 1) const
	{
		checkBandIndex(band);
		auto ndi = _noData.find(band);
		return (ndi == _noData.end())
			? static_cast<PixelData>(gdalNoData(band)) : ndi->second;
	}

protected:

	//////////////////////////////////////////////////
	// CachedGdal<PixelDataType>. Protected instance methods
	//////////////////////////////////////////////////

	/** Creates on heap a std::vector buffer for tile pixel data.
	 * @param latSize Pixel count in latitude direction
	 * @param lonSize Pixel count in longitude direction
	 * @return Address of created std::vector
	 */
	virtual void *createTileVector(int latSize, int lonSize) const
	{
		return new std::vector<PixelData>(latSize * lonSize);
	}

	/** Deletes tile vector.
	 * @param tileVector std::vector to delete
	 */
	virtual void deleteTileVector(void *tile) const
	{
		delete reinterpret_cast<std::vector<PixelData> *>(tile);
	}

	/** Returns address of tile's data buffer
	 * @param tileVector std::vector, containing tile pixel data
	 * @return Vector's buffer, containing pixel data
	 */
	virtual void *getTileBuffer(void *tile) const
	{
		return reinterpret_cast<std::vector<PixelData> *>(tile)->data();
	}

private:

	//////////////////////////////////////////////////
	// CachedGdal<PixelDataType>. Private instance data
	//////////////////////////////////////////////////

	/** Overridden no-data values */
	std::map<int, PixelData> _noData;
};

#endif /* CACHED_GDAL_H */
