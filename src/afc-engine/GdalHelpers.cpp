#include "GdalHelpers.h"
#include "MultiGeometryIterable.h"
#include "afclogging/ErrStream.h"
#include <ogr_feature.h>
#include <memory>
#include <exception>

template<>
OGRPoint * GdalHelpers::createGeometry(){
	return static_cast<OGRPoint *>(OGRGeometryFactory::createGeometry(wkbPoint));
}

template<>
OGRMultiPoint * GdalHelpers::createGeometry(){
	return static_cast<OGRMultiPoint *>(OGRGeometryFactory::createGeometry(wkbMultiPoint));
}

template<>
OGRLineString * GdalHelpers::createGeometry(){
	return static_cast<OGRLineString *>(OGRGeometryFactory::createGeometry(wkbLineString));
}

template<>
OGRMultiLineString * GdalHelpers::createGeometry(){
	return static_cast<OGRMultiLineString *>(OGRGeometryFactory::createGeometry(wkbMultiLineString));
}

template<>
OGRLinearRing * GdalHelpers::createGeometry(){
	return static_cast<OGRLinearRing *>(OGRGeometryFactory::createGeometry(wkbLinearRing));
}

template<>
OGRPolygon * GdalHelpers::createGeometry(){
	return static_cast<OGRPolygon *>(OGRGeometryFactory::createGeometry(wkbPolygon));
}

template<>
OGRMultiPolygon * GdalHelpers::createGeometry(){
	return static_cast<OGRMultiPolygon *>(OGRGeometryFactory::createGeometry(wkbMultiPolygon));
}

template<>
OGRGeometryCollection * GdalHelpers::createGeometry(){
	return static_cast<OGRGeometryCollection *>(OGRGeometryFactory::createGeometry(wkbGeometryCollection));
}

void GdalHelpers::OgrFreer::operator()(void *ptr) const{
	if(ptr){
		OGRFree(ptr);
	}
}

void GdalHelpers::GeometryDeleter::operator()(OGRGeometry *ptr) const{
	OGRGeometryFactory::destroyGeometry(ptr);
}

void GdalHelpers::FeatureDeleter::operator()(OGRFeature *obj){
	OGRFeature::DestroyFeature(obj);
}

void GdalHelpers::SrsDeleter::operator()(OGRSpatialReference *obj){
	OGRSpatialReference::DestroySpatialReference (obj);
}

void GdalHelpers::coalesce(OGRGeometryCollection *target, OGRGeometryCollection *source){
	for(OGRGeometry *item : MultiGeometryIterableMutable<OGRGeometry>(*source)){
		target->addGeometryDirectly(item);
	}
	while(!source->IsEmpty()){
		source->removeGeometry(0, FALSE);
	}
}


std::string GdalHelpers::exportWkb(const OGRGeometry &geom){
	const size_t wkbSize = geom.WkbSize();
	std::unique_ptr<unsigned char[]> wkbData(new unsigned char[wkbSize]);
	const int status = geom.exportToWkb(
			wkbXDR,
			wkbData.get()
			);
	if(status != OGRERR_NONE){
		throw std::runtime_error(ErrStream() << "Failed to export WKB: code " << status);
	}
	return std::string(reinterpret_cast<char *>(wkbData.get()), wkbSize);
}

std::string GdalHelpers::exportWkt(const OGRGeometry *geom){
	if(!geom){
		return std::string();
	}

	char *wktData;
	const int status = geom->exportToWkt(&wktData);
	if(status != OGRERR_NONE){
		throw std::runtime_error(ErrStream() << "Failed to export WKT: code " << status);
	}
	std::unique_ptr<char, OgrFreer> wrap(wktData);

	return std::string(wktData);
}

OGRGeometry * GdalHelpers::importWkt(const std::string &data){
	if(data.empty()){
		return nullptr;
	}

	OGRGeometry *geom = nullptr;
	std::string edit(data);
	char *front = const_cast<char *>(edit.data());
	const int status = OGRGeometryFactory::createFromWkt(&front, nullptr, &geom);
	if(status != OGRERR_NONE){
		delete geom;
		throw std::runtime_error(ErrStream() << "Failed to import WKT: code " << status);
	}
	return geom;
}

std::string GdalHelpers::exportWkt(const OGRSpatialReference *srs){
	if(!srs){
		return std::string();
	}

	char *wktData;
	const int status = srs->exportToWkt(&wktData);
	if(status != OGRERR_NONE){
		throw std::runtime_error(ErrStream() << "Failed to export WKT: code " << status);
	}
	std::unique_ptr<char, OgrFreer> wrap(wktData);

	return std::string(wktData);

}

std::string GdalHelpers::exportProj4(const OGRSpatialReference *srs){
	if(!srs){
		return std::string();
	}

	char *projData;
	const int status = srs->exportToProj4(&projData);
	std::unique_ptr<char, OgrFreer> wrap(projData);
	if(status != OGRERR_NONE){
		throw std::runtime_error(ErrStream() << "Failed to export Proj.4: code " << status);
	}

	return std::string(projData);
}

OGRSpatialReference * GdalHelpers::importWellKnownGcs(const std::string &name) {
	std::unique_ptr<OGRSpatialReference, SrsDeleter> srs(new OGRSpatialReference());
	const auto status = srs->SetWellKnownGeogCS(name.data());
	if (status != OGRERR_NONE) {
		throw std::runtime_error(ErrStream() << "Failed to import well-known GCS: code " << status);
	}
	return srs.release();
}
