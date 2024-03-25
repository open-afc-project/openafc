/*
 * Copyright (C) 2022 Broadcom. All rights reserved.
 * The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
 * that owns the software below.
 * This work is licensed under the OpenAFC Project License, a copy of which is
 * included with this software program.
 */
#include "GdalTransform.h"

#include <algorithm>
#include <cmath>
#include <gdal_priv.h>
#include <sstream>
#include <stdexcept>

///////////////////////////////////////////////////////////////////////////////

GdalTransform::BoundRect::BoundRect(double latDegMin_,
				    double lonDegMin_,
				    double latDegMax_,
				    double lonDegMax_) :
	latDegMin(latDegMin_), lonDegMin(lonDegMin_), latDegMax(latDegMax_), lonDegMax(lonDegMax_)
{
}

GdalTransform::BoundRect::BoundRect() : latDegMin(0), lonDegMin(0), latDegMax(0), lonDegMax(0)
{
}

bool GdalTransform::BoundRect::contains(double latDeg, double lonDeg) const
{
	lonDeg = rebaseLon(lonDeg, lonDegMin);
	return (latDegMin < latDeg) && (lonDegMin <= lonDeg) && (latDeg <= latDegMax) &&
	       (lonDeg < lonDegMax);
}

void GdalTransform::BoundRect::combine(const GdalTransform::BoundRect &other)
{
	latDegMin = std::min(latDegMin, other.latDegMin);
	lonDegMin = std::min(lonDegMin, other.lonDegMin);
	latDegMax = std::max(latDegMax, other.latDegMax);
	lonDegMax = std::max(lonDegMax, other.lonDegMax);
}

double GdalTransform::BoundRect::rebaseLon(double lon, double base)
{
	while ((lon - 360) >= base) {
		lon -= 360;
	}
	while (lon < base) {
		lon += 360;
	}
	return lon;
}

///////////////////////////////////////////////////////////////////////////////

GdalTransform::GdalTransform(GDALDataset *gdalDataSet_, const std::string &filename_) : margin(0)
{
	std::ostringstream errStr;

	// For GDAL files with latitude/longitude grid data meaning of
	// transformation coefficients is a s follows:
	// Longitude = gdalTransform(0) + PixelColumn*gdalTransform(1) + PixelRow*gdalTransform(2)
	// Latitude  = gdalTransform(3) + PixelColumn*gdalTransform(4) + PixelRow*gdalTransform(5)
	double gdalTransform[6];
	CPLErr gdalErr = gdalDataSet_->GetGeoTransform(gdalTransform);
	if (gdalErr != CPLErr::CE_None) {
		errStr << "ERROR: GdalTransform::GdalTransform(): "
			  "Failed to read transformation from GDAL data file '"
		       << filename_ << "': " << CPLGetLastErrorMsg();
		throw std::runtime_error(errStr.str());
	}
	if (!((gdalTransform[2] == 0) && (gdalTransform[4] == 0) && (gdalTransform[1] > 0) &&
	      (gdalTransform[5] < 0))) {
		errStr << "ERROR: GdalTransform::GdalTransform(): GDAL data file '" << filename_
		       << "': does not contain 'north up' data";
		throw std::runtime_error(errStr.str());
	}
	latPixPerDeg = -1. / gdalTransform[5];
	lonPixPerDeg = 1. / gdalTransform[1];
	latPixMax = -gdalTransform[3] / gdalTransform[5];
	lonPixMin = gdalTransform[0] / gdalTransform[1];
	latSize = gdalDataSet_->GetRasterYSize();
	lonSize = gdalDataSet_->GetRasterXSize();
}

GdalTransform::GdalTransform(const GdalTransform &gdalXform_,
			     int latPixOffset_,
			     int lonPixOffset_,
			     int latSize_,
			     int lonSize_) :
	latPixPerDeg(gdalXform_.latPixPerDeg),
	lonPixPerDeg(gdalXform_.lonPixPerDeg),
	latPixMax(gdalXform_.latPixMax - latPixOffset_),
	lonPixMin(gdalXform_.lonPixMin + lonPixOffset_),
	latSize(latSize_),
	lonSize(lonSize_),
	margin(0)
{
}

GdalTransform::GdalTransform() :
	latPixPerDeg(0),
	lonPixPerDeg(0),
	latPixMax(0),
	lonPixMin(0),
	latSize(0),
	lonSize(0),
	margin(0)
{
}

void GdalTransform::computePixel(double latDeg, double lonDeg, int *latIdx, int *lonIdx) const
{
	// Rebasing longitude relative to left side of bounding rectangle
	lonDeg = BoundRect::rebaseLon(lonDeg, (lonPixMin + margin) / lonPixPerDeg);

	*latIdx = (int)std::floor(latPixMax - latDeg * latPixPerDeg);
	*lonIdx = (int)std::floor(lonDeg * lonPixPerDeg - lonPixMin);

	// Off by more than a pixel - definitely a bug, not a rounding error
	if ((*latIdx < -1) || (*latIdx > latSize) || (*lonIdx < -1) || (*lonIdx > lonSize)) {
		auto br(makeBoundRect());
		std::ostringstream errStr;
		errStr << "ERROR: GdalTransform::computePixel() internal error: point (" << latDeg
		       << "N, " << lonDeg << "E is out of tile/GDAL bounds of [" << br.latDegMin
		       << " - " << br.latDegMax << "]N X [" << br.lonDegMin << " - " << br.lonDegMax
		       << "]E";
		throw std::runtime_error(errStr.str());
	}
	// Preventing rounding errors by less than a pixel
	*latIdx = std::max(0, std::min(latSize - 1, *latIdx));
	*lonIdx = std::max(0, std::min(lonSize - 1, *lonIdx));
}

GdalTransform::BoundRect GdalTransform::makeBoundRect() const
{
	return BoundRect((latPixMax - latSize + margin) / latPixPerDeg,
			 (lonPixMin + margin) / lonPixPerDeg,
			 (latPixMax - margin) / latPixPerDeg,
			 (lonPixMin + lonSize - margin) / lonPixPerDeg);
}

void GdalTransform::roundPpdToMultipleOf(double pixelsPerDegree)
{
	latPixPerDeg = std::round(latPixPerDeg / pixelsPerDegree) * pixelsPerDegree;
	lonPixPerDeg = std::round(lonPixPerDeg / pixelsPerDegree) * pixelsPerDegree;
	latPixMax = std::round(latPixMax / pixelsPerDegree) * pixelsPerDegree;
	lonPixMin = std::round(lonPixMin / pixelsPerDegree) * pixelsPerDegree;
}

void GdalTransform::setMarginsOutsideDeg(double deg)
{
	// ShiftUp ensures that fmod dividend is positive (latPixMax / latPixPerDeg
	// is latitude, minimum latitude is -90) - to ensure rounding down, and yet
	// does not affect fmod() result
	double shiftUp = std::round(100 / deg) * deg;
	margin = std::fmod(latPixMax / latPixPerDeg + shiftUp, deg) * latPixPerDeg;
	// Since there is no more than one extra pixel, margin should be a multiple
	// of 0.5
	margin = std::round(margin * 2) / 2;
}
