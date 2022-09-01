/*
 * Copyright (C) 2022 Broadcom. All rights reserved.
 * The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
 * that owns the software below.
 * This work is licensed under the OpenAFC Project License, a copy of which is
 * included with this software program.
 */

#ifndef GDAL_NAME_MAPPER_H
#define GDAL_NAME_MAPPER_H

#include "GdalTransform.h"
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

/** @file
 * Discovery of GDAL file name that corresponds to given latitude/longitude
 */

/** Abstract base class for multifile (tiled) directory file naming handlers.
 * Such handler class provides file name for given latitude/longitude, provides
 * fnmatch()-compatible filename pattern that matches all files
 */
class GdalNameMapperBase {
public:
	//////////////////////////////////////////////////
	// GdalNameMapperBase. Public instance methods
	//////////////////////////////////////////////////

	/** Virtual destructor */
	virtual ~GdalNameMapperBase() = default;

	/** Returns fnmatch-compatible filename pattern that matches all relevant
	 * GDAL files in the directory
	 */
	virtual std::string fnmatchPattern() const = 0;

	/** Provides file name for given latitude/longitude
	 * @param latDeg North-positive latitude in degrees
	 * @param lonDeg East-positive latitude in degrees
	 * @return File name for given coordinates, empty string if there is none
	 */
	virtual std::string nameFor(double latDeg, double lonDeg) = 0;
};

/** GDAL mapper, based on filename pattern.
 * Pattern is a string with {type:format} inserts, containing its variable parts.
 * Type is always a literal, format is variable and type-dependent. Currently
 * supported type:format pairs
 *	- latDegCeil:d-spec (int)fabs(ceil(latDeg)). Format is sprintf()-compatible
 *		variable part of d-format specification (without  leading '%' and
 *		trailing 'd')
 *	- latDegFloor:d-spec (int)fabs(floor1(latDeg)). Format is sprintf()-compatible
 *		variable part of d-format specification (without  leading '%' and
 *		trailing 'd')
 *	- latHem:NS Latitude hemisphere. Format is character to use for north and
 *		character to use for south
 *	- lonDegCeil:d-spec (int)fabs(ceil1(lonDeg)). Format is sprintf()-compatible
 *		variable part of d-format specification (without  leading '%' and
 *		trailing 'd')
 *	- lonDegFloor:d-spec (int)fabs(floor(lonDeg)). Format is sprintf()-compatible
 *		variable part of d-format specification (without  leading '%' and
 *		trailing 'd')
 *	- lonHem:EW Longitude hemisphere. Format is character to use for east and
 *		character to use for west
 * Since to avoid ambiguity this module when testing belonging point to rectangle
 * includes top and left boundaries, but excludes right and bottom ones, floor1()
 * and ceil1() differ from normal floor() and ceil on integer arguments:
 * floor1(N) == N-1, ceil1(N) == N+1 for integer N
 * Examples:
 *	- "USGS_1_{latHem:ns}{latDegCeil:}{lonHem:ew}{lonDegFloor:}*.tif" - 3DEP files
 *	- "{latHem:NS}{latDegFloor:02}{lonHem:EW}{lonDegFloor:03}.hgt" - SRTM files
 * Note that since SRTM uses latDegFloor, e.g. heights for points on 25N are
 * taken from N24....hgt
 * As of time of this writing globe file name mapping is too obscure - use
 * DirectGdalNameMapper for them
 */
class GdalNameMapperPattern: public GdalNameMapperBase {
public:
	//////////////////////////////////////////////////
	// GdalNameMapperPattern. Public instance methods
	//////////////////////////////////////////////////

	/** Construct with filename pattern.
	 * @param pattern Filename pattern (see comments to class)
	 * @param directory Directory containing files. Must be specified if pattern
	 *	contains wildcard symbols (*?[]), otherwise ignored
	 */
	GdalNameMapperPattern(const std::string &pattern,
		const std::string &directory = "");

	/** Virtual destructor */
	virtual ~GdalNameMapperPattern() = default;

	/** Returns fnmatch-compatible filename pattern that matches all relevant
	 * GDAL files in the directory
	 */
	virtual std::string fnmatchPattern() const;

	/** Provides file name for given latitude/longitude
	 * @param latDeg North-positive latitude in degrees
	 * @param lonDeg East-positive latitude in degrees
	 * @return File name for given coordinates, empty string if there is none
	 */
	virtual std::string nameFor(double latDeg, double lonDeg);

	//////////////////////////////////////////////////
	// GdalNameMapperPattern. Public instance methods
	//////////////////////////////////////////////////

	/** Creates pointer, passable to CachedGdal constructor.
	 * This function encapsulates C++11 hassle around unique
	 * pointer creation. Parameters are the same as constructor
	 * has.
	 * @param pattern Filename pattern (see comments to class)
	 * @param directory Directory containing files. Must be specified if pattern
	 *	contains wildcard symbols (*?[]), otherwise ignored
	 */
	static std::unique_ptr<GdalNameMapperBase> make_unique(
		const std::string &pattern, const std::string &directory = "");

private:

	//////////////////////////////////////////////////
	// GdalNameMapperPattern. Private constants
	//////////////////////////////////////////////////

	/** Source data for operation */
	enum class Src {
		Str,	/*!< String part */
		Lat,	/*!< Latitude */
		Lon,	/*<! Longitude */
	};

	/** Operation to perform */
	enum class Op {
		Literal,	/*!< Append string literal. String part is a literal */
		Hemi,		/*!< Append hemisphere. String part is [POS][NEG] */
		DegFloor,	/*!< Append fabs(floor(degree)). String part is '%...d' */
		DegCeil,	/*!< Append fabs(ceil(deg)). String part is '%...d' */
		DegFloor1,	/*!< Same as DegFloor, but integers rounded to N-1 */
		DegCeil1,	/*!< Same as DegCeil, but integers rounded to N+1 */
	};

	//////////////////////////////////////////////////
	// GdalNameMapperPattern. Private types
	//////////////////////////////////////////////////

	/** Part of name */
	struct NamePart {
		//////////////////////////////////////////////////
		// SimpleGdalNameMapper::NamePart. Public instance methods
		//////////////////////////////////////////////////

		/** Constructor.
		 * @param src Source data for operation
		 * @param op Operation to perform
		 * String str Literal for operation
		 */
		NamePart(Src src, Op op, std::string str);

		//////////////////////////////////////////////////
		// SimpleGdalNameMapper::NamePart. Public instance data
		//////////////////////////////////////////////////

		Src src;			/*!< Source data for operation */
		Op op;				/*!< Operation to perform */
		std::string str;	/*!< Literal for operation */
	};

	//////////////////////////////////////////////////
	// GdalNameMapperPattern. Private instance methods
	//////////////////////////////////////////////////

	/** Appends literal part of filename pattern to _nameParts and _fnmatchPattern.
	 * @param pattern Filename pattern passed to constructor
	 * @param offset Offset of literal part
	 * @param len Length of literal part
	 */
	void appendLiteral(std::string pattern, size_t offset, size_t len);

	/** Appends numeric part of filename pattern to _nameParts and _fnmatchPattern.
	 * @param src Data source
	 * @param op Data operation
	 * @param elemFormat Numeric part of printf-style number format
	 * @aram errPrefix Prefix for error messages
	 */
	void appendLatLon(Src src, Op op, const std::string &elemFormat,
		const std::string errPrefix);

	/** Filename pattern (see comments to class) */
	std::vector<NamePart> _nameParts;

	/** fnmatch() pattern */
	std::string _fnmatchPattern;

	/** Directory for the case of pattern with wildcards, empty otherwise */
	std::string _directory;

	/** Maps generated wildcarded filenames to real filenames */
	std::map<std::string, std::string> _wildcardMap;
};

/** GDAL mapper that obtains information from GDAL files in directory.
 * Employment of this mapper is not recommended (as it reads metadata from every
 * file during initialization) - especially on large directories, but can be
 * tolerated on data that was not properly tiled
 */
class GdalNameMapperDirect: public GdalNameMapperBase {
public:
	//////////////////////////////////////////////////
	// DirectGdalNameMapper. Public instance methods
	//////////////////////////////////////////////////

	/** Constructor
	 * @param fnmatchPattern Fnmatch-compatible pattern for relevant files
	 * @param directory Directory containing GDAL files
	 */
	GdalNameMapperDirect(const std::string &fnmatchPattern, const std::string &directory);

	/** Virtual destructor */
	virtual ~GdalNameMapperDirect() = default;

	/** Returns fnmatch-compatible filename pattern that matches all relevant
	 * GDAL files in the directory
	 */
	virtual std::string fnmatchPattern() const;

	/** Provides file name for given latitude/longitude
	 * @param latDeg North-positive latitude in degrees
	 * @param lonDeg East-positive latitude in degrees
	 * @return File name for given coordinates, empty string if there is none
	 */
	virtual std::string nameFor(double latDeg, double lonDeg);

	//////////////////////////////////////////////////
	// DirectGdalNameMapper. Public instance methods
	//////////////////////////////////////////////////

	/** Creates pointer, passable to CachedGdal constructor.
	 * This function encapsulates C++11 hassle around unique
	 * pointer creation. Parameters are the same as constructor
	 * has.
	 * @param fnmatchPattern Fnmatch-compatible pattern for relevant files
	 * @param directory Directory containing GDAL files
	 */
	static std::unique_ptr<GdalNameMapperBase> make_unique(
		const std::string &fnmatchPattern, const std::string &directory);

private:
	//////////////////////////////////////////////////
	// GdalNameMapperDirect. Private instance data
	//////////////////////////////////////////////////

	/** Pattern for relevant files */
	std::string _fnmatchPattern;

	/** Coordinate rectangles mapped to file names */
	std::vector<std::tuple<GdalTransform::BoundRect, std::string>> _files;
};

#endif /* GDAL_NAME_MAPPER_H */
