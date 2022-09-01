/*
 * Copyright (C) 2022 Broadcom. All rights reserved.
 * The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
 * that owns the software below.
 * This work is licensed under the OpenAFC Project License, a copy of which is
 * included with this software program.
 */

#include "GdalNameMapper.h"
#include <boost/filesystem.hpp>
#include <fnmatch.h>
#include <gdal/gdal_priv.h>
#include <boost/regex.hpp>
#include <sstream>
#include <stdexcept>

///////////////////////////////////////////////////////////////////////////////
// GdalNameMapperPattern::NamePart
///////////////////////////////////////////////////////////////////////////////

GdalNameMapperPattern::NamePart::NamePart(GdalNameMapperPattern::Src src_,
	GdalNameMapperPattern::Op op_, std::string str_)
	: src(src_), op(op_), str(str_)
{}

///////////////////////////////////////////////////////////////////////////////
// GdalNameMapperPattern
///////////////////////////////////////////////////////////////////////////////

GdalNameMapperPattern::GdalNameMapperPattern(const std::string &pattern,
	const std::string &directory)
{
	if (pattern.find_first_of("*?[]") != std::string::npos) {
		std::ostringstream errStr;
		if (directory.empty()) {
			errStr << "ERROR: GdalNameMapperPattern::GdalNameMapperPattern(): "
				"GDAL filename pattern contains wildcard, but directory "
				"is not specified";
			throw std::runtime_error(errStr.str());
		}
		if (!boost::filesystem::is_directory(directory)) {
			errStr << "ERROR: GdalNameMapperPattern::GdalNameMapperPattern(): "
				"Specified directory '" << directory << "' does not exist";
			throw std::runtime_error(errStr.str());
		}
		_directory = boost::filesystem::absolute(directory).string();
	} else {
		_directory = "";
	}
	boost::smatch m;
	boost::regex elemRegex("\\{(\\w+):(.*?)\\}");
	std::string lit;
	std::string::const_iterator start = pattern.begin(), end = pattern.end();
	while (boost::regex_search(start, end, m, elemRegex)) {
		appendLiteral(pattern, start - pattern.begin(), m.position());
		start = m[0].second;
		std::string elemType = m[1].str(), elemFormat = m[2].str();
		std::ostringstream errPrefix;
		errPrefix << "ERROR: GdalNameMapperPattern::GdalNameMapperPattern(): "
			"Invalid format for element '" << m.str() <<
			"' in filename pattern '" << pattern << "'";
		if ((elemType == "latHem") || (elemType == "lonHem")) {
			if (elemFormat.length() != 2) {
				throw std::runtime_error(errPrefix.str() +
					": hemisphere specifier must be two character long");
			}
			_nameParts.emplace_back((elemType.substr(0, 3) == "lat") ? Src::Lat : Src::Lon,
				Op::Hemi, elemFormat);
			_fnmatchPattern += "[" + elemFormat + "]";
		} else if (elemType == "latDegFloor") {
			appendLatLon(Src::Lat, Op::DegFloor1, elemFormat, errPrefix.str());
		} else if (elemType == "latDegCeil") {
			appendLatLon(Src::Lat, Op::DegCeil, elemFormat, errPrefix.str());
		} else if (elemType == "lonDegFloor") {
			appendLatLon(Src::Lon, Op::DegFloor, elemFormat, errPrefix.str());
		} else if (elemType == "lonDegCeil") {
			appendLatLon(Src::Lon, Op::DegCeil1, elemFormat, errPrefix.str());
		} else {
			throw std::runtime_error(errPrefix.str());
		}
	}
	appendLiteral(pattern, start - pattern.begin(), end - start);
}

void GdalNameMapperPattern::appendLiteral(std::string pattern, size_t pos, size_t len)
{
	if (len == 0) {
		return;
	}
	std::string lit = pattern.substr(pos, len);
	if ((lit.find('{') != std::string::npos) || (lit.find('{') != std::string::npos)) {
		std::ostringstream errStr;
		errStr << "ERROR: GdalNameMapperPattern::appendLiteral(): "
			"Filename pattern '" << pattern <<
			"' contains unrecognized element at offset " << pos;
		throw std::runtime_error(errStr.str());
	}
	_nameParts.emplace_back(Src::Str, Op::Literal, lit);
	_fnmatchPattern += lit;
}

void GdalNameMapperPattern::appendLatLon(Src src, Op op, const std::string &elemFormat,
	const std::string errPrefix)
{
	if (elemFormat.find('%') != std::string::npos) {
		throw std::runtime_error(errPrefix + ": format should not contain '%' character");
	}
	char buf[50];
	std::string printfFormat = "%" + elemFormat + "d";
	int n = snprintf(buf, sizeof(buf), printfFormat.c_str(), 0);
	if ((n <= 0) || (n >= (int)sizeof(buf))) {
		throw std::runtime_error(errPrefix);
	}
	_nameParts.emplace_back(src, op, printfFormat);
	if ((elemFormat.length() >= 2) && (elemFormat[0] == '0')) {
		for (int i = atoi(elemFormat.substr(1, 1).c_str()); i--;
			_fnmatchPattern += "[0-9]");
	} else {
		_fnmatchPattern += "*";
	}
}

std::string GdalNameMapperPattern::fnmatchPattern() const
{
	return _fnmatchPattern;
}

std::string GdalNameMapperPattern::nameFor(double latDeg, double lonDeg)
{
	for (const NamePart &np: _nameParts) {
		double *src = (np.src == Src::Lat) ? &latDeg : &lonDeg;
		switch (np.op) {
		case Op::DegCeil1:
			if (*src == std::round(*src)) {
				*src += 1;
				if ((np.src == Src::Lon) && (*src == 181)) {
					*src = -179;
				}
			}
			break;
		case Op::DegFloor1:
			if (*src == std::round(*src)) {
				*src -= 1;
				if ((np.src == Src::Lon) && (*src == -181)) {
					*src = 179;
				}
			}
			break;
		default:
			break;
		}
	}
	std::string ret;
	for (const NamePart &np: _nameParts) {
		char buf[50];
		switch (np.op) {
		case Op::Literal:
			ret += np.str;
			break;
		case Op::Hemi:
			ret += np.str[((np.src == Src::Lat) ? latDeg : lonDeg) < 0];
			break;
		case Op::DegCeil1:
		case Op::DegCeil:
			snprintf(buf, sizeof(buf), np.str.c_str(),
				(int)fabs(ceil((np.src == Src::Lat) ? latDeg : lonDeg)));
			ret += buf;
			break;
		case Op::DegFloor:
		case Op::DegFloor1:
			snprintf(buf, sizeof(buf), np.str.c_str(),
				(int)fabs(floor((np.src == Src::Lat) ? latDeg : lonDeg)));
			ret += buf;
			break;
		}
	}
	if (!ret.empty() && !_directory.empty()) {
		// Source pattern - and hence generated pattern - contains wildcard.
		// So real file name should be found

		// Maybe it was found previously?
		auto i = _wildcardMap.find(ret);
		if (i != _wildcardMap.end()) {
			return i->second;
		}
		// So it should be looked up in directory - will lookup for lexicographically
		// largest candidate (to exclude ambiguity and to cater for 3DEP)
		std::string candidate = "";
		for (boost::filesystem::directory_iterator di(_directory);
			di != boost::filesystem::directory_iterator(); ++di)
		{
			std::string filename = di->path().filename().string();
			if (boost::filesystem::is_regular_file(di->path()) &&
				(fnmatch(ret.c_str(), filename.c_str(), 0) != FNM_NOMATCH) &&
				(candidate.empty() || (candidate < filename)))
			{
				candidate = filename;
			}
		}
		_wildcardMap[ret] = candidate;
		return candidate;
	}
	return ret;
}

std::unique_ptr<GdalNameMapperBase> GdalNameMapperPattern::make_unique(
		const std::string &pattern, const std::string &directory)
{
	return std::unique_ptr<GdalNameMapperBase>(new GdalNameMapperPattern(pattern, directory));
}

///////////////////////////////////////////////////////////////////////////////
// GdalNameMapperDirect
///////////////////////////////////////////////////////////////////////////////

GdalNameMapperDirect::GdalNameMapperDirect(const std::string &fnmatchPattern,
	const std::string &directory)
	: _fnmatchPattern(fnmatchPattern)
{
	GDALAllRegister();
	std::ostringstream errStr;
	for (boost::filesystem::directory_iterator di(directory);
		di != boost::filesystem::directory_iterator(); ++di)
	{
		std::string filename = di->path().filename().native();
		if ((!boost::filesystem::is_regular_file(di->path())) ||
			(fnmatch(fnmatchPattern.c_str(), filename.c_str(), 0)
			== FNM_NOMATCH))
		{
			continue;
		}
		GDALDataset *gdalDataSet = nullptr;
		try {
			GDALDataset *gdalDataSet =
				static_cast<GDALDataset*>(GDALOpen(di->path().native().c_str(), GA_ReadOnly));
			_files.push_back(
				std::make_tuple(GdalTransform(gdalDataSet, filename).makeBoundRect(), filename));
			GDALClose(gdalDataSet);
		} catch (...) {
			GDALClose(gdalDataSet);
			throw;
		}
	}
}

std::string GdalNameMapperDirect::fnmatchPattern() const
{
	return _fnmatchPattern;
}

std::string GdalNameMapperDirect::nameFor(double latDeg, double lonDeg)
{
	for (auto &fi: _files) {
		if (std::get<0>(fi).contains(latDeg, lonDeg)) {
			return std::get<1>(fi);
		}
	}
	return "";
}
std::unique_ptr<GdalNameMapperBase> GdalNameMapperDirect::make_unique(
		const std::string &fnmatchPattern, const std::string &directory)
{
	return std::unique_ptr<GdalNameMapperBase>(
		new GdalNameMapperDirect(fnmatchPattern, directory));
}
